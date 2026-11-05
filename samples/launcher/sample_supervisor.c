/**
 * @file    sample_supervisor.c
 * @brief   Background service supervisor implementation, see sample_supervisor.h.
 */
#include "sample_supervisor.h"
#include "sample_registry.h"
#include "sample_lifecycle.h"

#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"

#include <stdlib.h>
#include <string.h>

#define SUP_MAX       8    /* max concurrent services (widget/upgrade/… — a handful) */
#define SUP_MAX_ARGV  8    /* argv[0]=name + up to 7 extra tokens */

/* Slots never move while a service runs (the running thread holds &slot and slot.ctx.argv points
 * into slot.argv), so a free-flag array is used rather than a packed vector. */
typedef struct {
    bool                used;
    volatile int        done;        /* set by the thread when run() returned on its own */
    const SampleEntry  *entry;
    SampleStop         *stop;
    T_ThingModelThread  thread;
    SampleCtx           ctx;
    char               *argv[SUP_MAX_ARGV];
} SupSvc;

struct SampleSupervisor {
    ThingModelClientSession_t              *shared;
    const ThingModelClientSession_Config_t *cfg;
    SupSvc                                  svc[SUP_MAX];
};

SampleSupervisor *sample_supervisor_create(ThingModelClientSession_t *shared,
                                           const ThingModelClientSession_Config_t *cfg)
{
    SampleSupervisor *sv = (SampleSupervisor *)calloc(1, sizeof(*sv));
    if (!sv) return NULL;
    sv->shared = shared;
    sv->cfg    = cfg;
    return sv;
}

static SupSvc *sup_find(const SampleSupervisor *sv, const char *name)
{
    for (int i = 0; i < SUP_MAX; ++i)
        if (sv->svc[i].used && sv->svc[i].entry && strcmp(sv->svc[i].entry->name, name) == 0)
            return (SupSvc *)&sv->svc[i];
    return NULL;
}

bool sample_supervisor_is_running(const SampleSupervisor *sv, const char *name)
{
    return sv && name && sup_find(sv, name) != NULL;
}

int sample_supervisor_count(const SampleSupervisor *sv)
{
    int n = 0;
    if (sv) for (int i = 0; i < SUP_MAX; ++i) if (sv->svc[i].used) n++;
    return n;
}

/* Background thread body: bind this activity's stop token so the sample's sample_should_stop()
 * polls *our* token, then run the sample's ordinary entry on the shared session. */
static void sup_thread(void *arg)
{
    SupSvc *s = (SupSvc *)arg;
    sample_stop_bind_current(s->stop);
    int rc = s->entry->run(&s->ctx);
    LOG_INFO("[svc] %s exited (rc=%d)", s->entry->name, rc);
    s->done = 1;
}

int sample_supervisor_start(SampleSupervisor *sv, const char *name,
                            int extra_argc, char *const *extra_argv)
{
    if (!sv || !name) return -1;
    const SampleEntry *e = sample_registry_find(name);
    if (!e || e->kind != SAMPLE_KIND_SERVICE) return -1;
    if (sample_supervisor_is_running(sv, name)) return -2;

    SupSvc *s = NULL;
    for (int i = 0; i < SUP_MAX; ++i) if (!sv->svc[i].used) { s = &sv->svc[i]; break; }
    if (!s) return -3;

    memset(s, 0, sizeof(*s));
    s->entry = e;
    s->stop  = sample_stop_create();
    if (!s->stop) return -3;

    int ai = 0;
    s->argv[ai++] = (char *)e->name;
    for (int i = 0; i < extra_argc && ai < SUP_MAX_ARGV - 1; ++i)
        s->argv[ai++] = extra_argv[i];
    s->argv[ai] = NULL;

    s->ctx.cfg        = sv->cfg;
    s->ctx.session    = sv->shared;   /* services always run on the shared session */
    s->ctx.background = true;         /* suppress interactive-only preamble (Phase-A dry-run) */
    s->ctx.argc       = ai;
    s->ctx.argv       = s->argv;
    s->used           = true;

    s->thread = ThingModelOsal_ThreadCreate(sup_thread, s, e->name, 0);
    if (!s->thread._h) {
        sample_stop_destroy(s->stop);
        memset(s, 0, sizeof(*s));      /* used=false */
        LOG_ERROR("[svc] %s thread_create failed", name);
        return -3;
    }
    LOG_INFO("[svc] %s started", e->name);
    return 0;
}

void sample_supervisor_print(const SampleSupervisor *sv)
{
    if (!sv) return;
    int n = sample_supervisor_count(sv);
    if (n == 0) { printf("  (no background services running)\n"); return; }
    for (int i = 0; i < SUP_MAX; ++i) {
        const SupSvc *s = &sv->svc[i];
        if (!s->used) continue;
        printf("  %-12s %s\n", s->entry->name, s->done ? "exited (join pending)" : "running");
    }
}

static void sup_join_free(SupSvc *s)
{
    ThingModelOsal_ThreadJoin(s->thread);
    sample_stop_destroy(s->stop);
    memset(s, 0, sizeof(*s));          /* used=false */
}

int sample_supervisor_stop(SampleSupervisor *sv, const char *name)
{
    if (!sv || !name) return -1;
    SupSvc *s = sup_find(sv, name);
    if (!s) return -1;
    LOG_INFO("[svc] stopping %s ...", name);
    sample_stop_signal(s->stop);
    sup_join_free(s);
    return 0;
}

void sample_supervisor_stop_all(SampleSupervisor *sv)
{
    if (!sv) return;
    /* Signal everyone first (so they wind down in parallel), then join. */
    for (int i = 0; i < SUP_MAX; ++i)
        if (sv->svc[i].used) sample_stop_signal(sv->svc[i].stop);
    for (int i = 0; i < SUP_MAX; ++i)
        if (sv->svc[i].used) sup_join_free(&sv->svc[i]);
}

void sample_supervisor_destroy(SampleSupervisor *sv)
{
    if (!sv) return;
    sample_supervisor_stop_all(sv);
    free(sv);
}

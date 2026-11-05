/**
 * @file    sample_lifecycle.c
 * @brief   Cooperative cancellation model implementation, see sample_lifecycle.h.
 */
#include "sample_lifecycle.h"

#include "thing_model_client/thing_model_client_osal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  include <signal.h>
#  define SAMPLE_TLS _Thread_local
#else
/* RTOS: the supervisor (and thus token binding) is POSIX-only; no per-task override is needed, so a
 * plain unused global keeps this portable to toolchains without thread-local storage. */
#  define SAMPLE_TLS
#endif

static volatile int g_stop = 0;          /* foreground / global stop (set by SIGINT) */
static uint64_t s_deadline_us = 0;       /* 0 = no deadline (foreground/global path only) */
static SAMPLE_TLS SampleStop *s_tls_stop = NULL;   /* bound token for the calling thread, if any */

struct SampleStop { volatile int flag; };

SampleStop *sample_stop_create(void)          { return (SampleStop *)calloc(1, sizeof(struct SampleStop)); }
void        sample_stop_destroy(SampleStop *s){ free(s); }
void        sample_stop_signal(SampleStop *s) { if (s) s->flag = 1; }
bool        sample_stop_is_requested(const SampleStop *s) { return s && s->flag != 0; }
void        sample_stop_bind_current(SampleStop *s)       { s_tls_stop = s; }

void sample_request_stop(void) { g_stop = 1; }
/* Only clear the stop flag; the deadline is owned by sample_deadline_arm/cancel — do NOT reset it here.
 * Otherwise the per-sample budget that autorun arms just before run_entry would be wiped by the
 * clear_stop() call inside run_entry, so observer samples (e.g. power's while(!should_stop) sleep(30s))
 * would never time out and the whole run would hang. */
void sample_clear_stop(void)   { g_stop = 0; }
bool sample_should_stop(void)
{
    /* A background activity polls its OWN bound token (no shared deadline). */
    if (s_tls_stop) return s_tls_stop->flag != 0;
    /* Foreground / direct mode: the global token, plus the optional auto-cycle deadline. */
    if (s_deadline_us && ThingModelOsal_TimeUs() >= s_deadline_us) g_stop = 1;
    return g_stop != 0;
}

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
static void on_sigint(int sig) { (void)sig; g_stop = 1; }
#endif

void sample_lifecycle_install(void)
{
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
}

void sample_deadline_arm(unsigned seconds)
{
    if (!seconds) return;
    s_deadline_us = ThingModelOsal_TimeUs() + (uint64_t)seconds * 1000000u;
}

void sample_deadline_cancel(void)
{
    s_deadline_us = 0;
}

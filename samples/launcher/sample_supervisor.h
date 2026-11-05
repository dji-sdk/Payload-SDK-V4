/**
 * @file    sample_supervisor.h
 * @brief   Background service supervisor — runs SAMPLE_KIND_SERVICE samples concurrently on ONE
 *          shared session, so device-facing services (widget, upgrade) stay resident while the
 *          foreground shell fires one-off actions on the same session (no reconnect between them).
 *
 *   This mirrors what the RTOS firmware already does in launcher_task.c (one session + several
 *   *_run_with_session background tasks), lifted to a small reusable unit on the POSIX side.
 *   Concurrency uses the OSAL thread/stop primitives; each service gets its own cooperative-stop
 *   token (see sample_lifecycle.h), so stopping one — or a foreground Ctrl-C — never disturbs the
 *   others.
 */
#ifndef SAMPLE_SUPERVISOR_H
#define SAMPLE_SUPERVISOR_H

#include <stdbool.h>
#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SampleSupervisor SampleSupervisor;

/** Create a supervisor bound to the launcher's shared session + config. Returns NULL on OOM. */
SampleSupervisor *sample_supervisor_create(ThingModelClientSession_t *shared,
                                           const ThingModelClientSession_Config_t *cfg);

/**
 * Start service <name> (must be registered as SAMPLE_KIND_SERVICE) as a background thread on the
 * shared session. extra_argv[0..extra_argc-1] are appended after argv[0]=name (e.g. to hand a
 * service a non-default HTTP port). The pointed-to strings must outlive the service (autostart
 * passes string literals; the shell passes none).
 * @return 0 ok; -1 unknown / not a service; -2 already running; -3 capacity/thread failure.
 */
int  sample_supervisor_start(SampleSupervisor *sv, const char *name,
                             int extra_argc, char *const *extra_argv);

bool sample_supervisor_is_running(const SampleSupervisor *sv, const char *name);
int  sample_supervisor_count(const SampleSupervisor *sv);       /* number of live services */
void sample_supervisor_print(const SampleSupervisor *sv);       /* human-readable status line(s) */

int  sample_supervisor_stop(SampleSupervisor *sv, const char *name);   /* signal + join one; 0/-1 */
void sample_supervisor_stop_all(SampleSupervisor *sv);                 /* signal all, then join all */
void sample_supervisor_destroy(SampleSupervisor *sv);                  /* stop_all + free */

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_SUPERVISOR_H */

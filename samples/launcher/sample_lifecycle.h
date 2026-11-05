/**
 * @file    sample_lifecycle.h
 * @brief   Cooperative cancellation model for the single-bin sample.
 *
 *   Multiple samples reuse the same process/firmware sequentially: a sample's run() must be able to
 *   "finish and return to the menu"; exit() is strictly forbidden. A stop signal is agreed:
 *     - the launcher calls sample_clear_stop() to reset before each run();
 *     - the platform translates "user wants to interrupt" into sample_request_stop()
 *         · Linux: this module installs a SIGINT/SIGTERM handler that sets it automatically;
 *         · MCU  : when UART receives 'q'/Ctrl-C, the console layer calls sample_request_stop();
 *     - a sample's long/infinite loop writes its condition as `!sample_should_stop()`, finishing and returning once set.
 */
#ifndef SAMPLE_LIFECYCLE_H
#define SAMPLE_LIFECYCLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sample_lifecycle_install(void);  /* install platform interrupt hooks (Linux=signal; MCU=empty impl) */
void sample_request_stop(void);       /* request the current sample to stop */
void sample_clear_stop(void);         /* reset (launcher calls before each run) */
bool sample_should_stop(void);        /* sample loop-condition query */

/* ── Per-activity cooperative-stop token ──────────────────────────────────────────────
 * The single global stop above is the FOREGROUND token: SIGINT/Ctrl-C sets it, and interactive/
 * direct-mode samples poll it via sample_should_stop(). When several activities run at once (the
 * supervisor runs background services concurrently with a foreground sample on one shared session),
 * a single global would let a foreground Ctrl-C kill the background services too. So each activity
 * gets its OWN token, bound to the thread that runs it; sample_should_stop() then resolves to the
 * calling thread's token when one is bound, and to the global foreground token otherwise. Samples do
 * not change — they keep calling sample_should_stop().
 *   Linux only: on RTOS no token is ever bound (the supervisor is POSIX-only), so behavior is unchanged. */
typedef struct SampleStop SampleStop;
SampleStop *sample_stop_create(void);
void        sample_stop_destroy(SampleStop *s);
void        sample_stop_signal(SampleStop *s);            /* request THIS activity to stop */
bool        sample_stop_is_requested(const SampleStop *s);
void        sample_stop_bind_current(SampleStop *s);      /* bind token to the calling thread (NULL to unbind) */

/* For auto-cycling: after `seconds`, sample_should_stop() starts returning true, so blocking/infinite
 * samples finish and advance to the next. Implemented by comparing a target monotonic timestamp inside
 * sample_should_stop() (which every sample loop already polls) — no OS timer, works on host and MCU alike.
 *   seconds=0 means no limit (only suitable for samples that finish on their own). */
void sample_deadline_arm(unsigned seconds);  /* seconds=0 means no limit */
void sample_deadline_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_LIFECYCLE_H */

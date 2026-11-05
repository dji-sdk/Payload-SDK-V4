/*
 * sample_session.h — sample session facade.
 *
 * Collects "Connect + all device-level common bootstrap + symmetric teardown" into a pair of functions,
 * so each sample opens a session with all common pieces wired up in one line, without manually bringing up
 * device_report / bind / … one by one (a new device-level cross-cutting concern only changes this facade, samples untouched).
 *
 *   Typical usage:
 *     ThingModelClientSession_t *s = sample_session_open(&cfg);
 *     if (!s) { ...handle connection failure... }
 *     ... business ...
 *     sample_session_close(s);
 *
 * Notes:
 *   - This is a sample-app assembly facade, not SDK/runtime behavior.
 *   - Samples with special session needs (dry-run without a broker, multi-session/multi-SN stress, etc.) may bypass this facade
 *     and call ThingModelClientSession_Connect + only the common modules they need directly.
 */
#ifndef SAMPLE_SESSION_H
#define SAMPLE_SESSION_H

#include "thing_model_client/thing_model_client_session.h"
#include "sample_registry.h"   /* SampleCtx */

#ifdef __cplusplus
extern "C" {
#endif

/** Connect and bring up all device-level common pieces (device_report + bind listener + future concerns).
 *  @return the session on success; returns NULL on connection failure (starts no common piece). */
ThingModelClientSession_t *sample_session_open(const ThingModelClientSession_Config_t *cfg);

/** Symmetric teardown: stop all common pieces and Disconnect. No-op if s==NULL. */
void sample_session_close(ThingModelClientSession_t *s);

/**
 * Get the session a sample should use, without every sample re-implementing "reuse-or-open".
 *   - If the launcher provided a shared session (ctx->session != NULL) AND the sample did not override
 *     identity (cfg->device_sn equals the shared session's SN), the shared session is reused and
 *     *owned is set false — no new broker dial, so switching samples never reconnects.
 *   - Otherwise a fresh session is opened via sample_session_open() and *owned is set true.
 * Pair every acquire with sample_session_release(s, owned).
 * @return the session to use, or NULL if a fresh open was needed and failed.
 */
ThingModelClientSession_t *sample_session_acquire(const SampleCtx *ctx,
                                                  const ThingModelClientSession_Config_t *cfg,
                                                  bool *owned);

/** Release a session obtained from sample_session_acquire(): closes it iff owned==true; else a no-op
 *  (the shared session outlives the sample and is closed by the launcher at process exit). */
void sample_session_release(ThingModelClientSession_t *s, bool owned);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_SESSION_H */

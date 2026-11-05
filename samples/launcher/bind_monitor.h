/*
 * bind_monitor.h — shared device bind-state listener (used by all samples).
 *
 * Bind state is device-level info spanning caps: after the device connects to the broker,
 * if the cloud finds it unbound it pushes bind_required on the events channel
 * (bare method name, no cap prefix).
 *
 * This module uses the runtime's AddListener hook
 * to register a listener and subscribe to events, and decodes
 * with the protocol decoder
 * BindJson_DecodeBindRequired() — the schema's single source of truth lives in the bind schema,
 * and the runtime stays protocol-agnostic. Business code can query bind_monitor_is_bound() to gate before issuing commands.
 *
 * Usage (one line each after connect and before Disconnect, alongside device_report):
 *   ThingModelClientSession_t *s = ThingModelClientSession_Connect(&cfg);
 *   bind_monitor_start(s);
 *   ...
 *   bind_monitor_stop();
 *   ThingModelClientSession_Disconnect(s);
 *
 * start/stop are idempotent: repeated start is ignored, stop is a no-op when not started.
 * Threads: the callback is invoked by the transport background thread; the flag is volatile,
 * reads/writes are atomic word-size, so no extra lock is needed.
 */
#ifndef BIND_MONITOR_H
#define BIND_MONITOR_H

#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Register the bind-state listener: AddListener + subscribe to events. @return 0 on success (also returns 0 if already running). */
int  bind_monitor_start(ThingModelClientSession_t *session);

/** Unregister the listener (no-op if not started). */
void bind_monitor_stop(void);

/** Whether the device is currently bound (= most recent bind_state==BOUND).
 *  Returns true if no bind push has ever been received (conservatively allow: the cloud only pushes
 *  proactively on anomalies, so no push = treated as no need to block; for strict checks use bind_monitor_got_push below). */
bool bind_monitor_is_bound(void);

/** The raw value of the most recent bind_state; returns 0 (BIND_STATE_IDLE) if never received. */
int  bind_monitor_state(void);

/** Whether this session has received at least one bind push. */
bool bind_monitor_got_push(void);

#ifdef __cplusplus
}
#endif

#endif /* BIND_MONITOR_H */

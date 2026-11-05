/*
 * sample_aircraft_info.h — shared aircraft-info listener (used by all samples).
 *
 * Aircraft info is device-level OSD spanning caps: after the device connects to the broker,
 * the cloud/App periodically pushes aircraft_info_push on the OSD channel
 * thing/product/<sn>/osd (bare method name, no cap prefix), carrying aircraft type /
 * aircraft firmware version / mobile App info / on-board device list.
 *
 * This module uses the runtime's AddListener hook
 * to register a listener and subscribe to osd, and decodes
 * with the protocol decoder
 * AircraftInfoJson_DecodeAircraftInfoPush() — the schema's single source of truth lives in
 * the aircraft_info schema, and the runtime stays protocol-agnostic.
 *
 * === Toggle (enabled) ===
 * By default all samples subscribe, but this can be toggled. The toggle is a
 * process-level flag; its initial value is seeded by priority:
 *     env var THING_MODEL_AIRCRAFT_INFO (0/off/false->off, otherwise->on)
 *       > compile-time macro SAMPLE_AIRCRAFT_INFO_DEFAULT (default 1=on)
 * All three front doors are covered:
 *   - Interactive menu: a key at runtime calls sample_aircraft_info_set_enabled() to toggle live;
 *   - autorun / CI: set THING_MODEL_AIRCRAFT_INFO=0 to disable, zero input;
 *   - MCU / RTOS: with no getenv, falls back to the compile-time macro SAMPLE_AIRCRAFT_INFO_DEFAULT.
 * Since each sample opens/closes its own session, and the menu sits between two runs (no active session),
 * a menu toggle only changes the flag, taking effect at the next sample_session_open -> sample_aircraft_info_start.
 *
 * Usage (hooked into the session facade, see common/sample_session.c, alongside bind_monitor):
 *   ThingModelClientSession_t *s = ThingModelClientSession_Connect(&cfg);
 *   sample_aircraft_info_start(s);   // only actually subscribes when enabled
 *   ...
 *   sample_aircraft_info_stop();
 *   ThingModelClientSession_Disconnect(s);
 *
 * start/stop are idempotent: repeated start is ignored, stop is a no-op when not started.
 * Threads: the callback is invoked by the transport background thread; the flag is volatile,
 * reads/writes are atomic word-size, so no extra lock is needed in the sample scenario.
 */
#ifndef SAMPLE_AIRCRAFT_INFO_H
#define SAMPLE_AIRCRAFT_INFO_H

#include <stdbool.h>
#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Register the aircraft-info listener: when enabled, AddListener + subscribe to osd; when disabled, return without subscribing.
 *  @return 0 on success (also returns 0 if already running, or if disabled and thus not subscribed). */
int  sample_aircraft_info_start(ThingModelClientSession_t *session);

/** Unregister the listener (no-op if not started). */
void sample_aircraft_info_stop(void);

/** Set the toggle (takes effect on next start; does not affect the already-running current session). */
void sample_aircraft_info_set_enabled(bool on);

/** Query the current toggle (on first call, lazily seeds the initial value from env / macro). */
bool sample_aircraft_info_enabled(void);

/** Whether this session has received at least one aircraft_info_push. */
bool sample_aircraft_info_got_push(void);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_AIRCRAFT_INFO_H */

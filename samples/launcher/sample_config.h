/**
 * @file    sample_config.h
 * @brief   Common sample scaffold — the "single source" for the session connection config.
 *
 *   Each demo (flight / camera / efts / time_sync ...) no longer repeats the
 *   ThingModelClientSession_Config_t assignments itself; they all call this module to load them.
 *
 *   Value sources (priority):
 *     env var THING_MODEL_* (from dev.env sourced into the shell / CI / command line)
 *       > this module's built-in placeholder defaults
 *
 *   Put real credentials in dev.env at the repo root (already .gitignore'd); before use:
 *     set -a; . ./dev.env; set +a
 *
 *   Note: getenv use is isolated to this sample module; the SDK core depends on no
 *        environment convention.
 */
#ifndef THING_MODEL_SAMPLE_CONFIG_H
#define THING_MODEL_SAMPLE_CONFIG_H

#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * RTOS-safe env var read: if getenv exists and returns a non-empty value, return it; otherwise return def.
 * Demos reading their own args (target device id / observation duration etc.) also reuse this function,
 * avoiding each defining its own getenv wrapper.
 */
const char *thing_model_sample_getenv(const char *key, const char *def);

/**
 * Load the session config: filled from THING_MODEL_* env vars (default=placeholder), and presets
 *   ports 8883/1883, auto-reconnect 1..30s, topic_style default (V2), default transport/crypto.
 *
 * The string pointers inside cfg point at env vars or static literals, with a lifetime spanning the
 * whole process, always valid during ThingModelClientSession_Connect(); the caller need not free them.
 *
 * @param cfg output, zeroed first then filled; returns immediately if NULL.
 */
void thing_model_sample_config_load(ThingModelClientSession_Config_t *cfg);

/**
 * Print a redacted config summary: broker / app_id / device_sn in plaintext;
 * app_key / app_license masked (indicates "not set" if placeholder). Handy for startup self-check.
 */
void thing_model_sample_config_print(const ThingModelClientSession_Config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* THING_MODEL_SAMPLE_CONFIG_H */

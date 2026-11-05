/**
 * @file    sample_config.c
 * @brief   Common sample scaffolding implementation, see sample_config.h.
 */
#include "sample_config.h"

#include <stdio.h>
#include <string.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  include <stdlib.h>            /* getenv */
#  define THING_MODEL_SAMPLE_HAS_GETENV 1
#endif

/* ── Optional local access info (not committed to the repo) ──────────
 * During development, put app_key / app_license / broker etc. into
 * sample_config_local.h in the same directory (see sample_config_local.h.example);
 * it takes effect automatically at compile time, so after building you can
 * connect to the broker directly without setting environment variables each time.
 *
 * Value priority: env var THING_MODEL_*  >  sample_config_local.h  >  placeholder default */
#if defined(__has_include)
#  if __has_include("sample_config_local.h")
#    include "sample_config_local.h"
#  endif
#endif

#ifndef SAMPLE_DEFAULT_APP_ID
#  define SAMPLE_DEFAULT_APP_ID      "YOUR_APP_ID"
#endif
#ifndef SAMPLE_DEFAULT_APP_NAME
#  define SAMPLE_DEFAULT_APP_NAME    "YOUR_APP_NAME"
#endif
#ifndef SAMPLE_DEFAULT_APP_KEY
#  define SAMPLE_DEFAULT_APP_KEY     "YOUR_APP_KEY"
#endif
#ifndef SAMPLE_DEFAULT_APP_LICENSE
#  define SAMPLE_DEFAULT_APP_LICENSE "YOUR_APP_LICENSE"
#endif
#ifndef SAMPLE_DEFAULT_DEVICE_SN
#  define SAMPLE_DEFAULT_DEVICE_SN   "YOUR_DEVICE_SN"
#endif
#ifndef SAMPLE_DEFAULT_BROKER
#  define SAMPLE_DEFAULT_BROKER      "127.0.0.1"
#endif
#ifndef SAMPLE_DEFAULT_CA_CERT
#  define SAMPLE_DEFAULT_CA_CERT     "/path/to/ca.crt"
#endif

const char *thing_model_sample_getenv(const char *key, const char *def)
{
#ifdef THING_MODEL_SAMPLE_HAS_GETENV
    const char *v = getenv(key);
    return (v && *v) ? v : def;
#else
    (void)key;
    return def;
#endif
}

void thing_model_sample_config_load(ThingModelClientSession_Config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* ── Access authentication (real values go in local sample_config_local.h or env vars, default=placeholder) ── */
    cfg->app_id      = thing_model_sample_getenv("THING_MODEL_APP_ID",      SAMPLE_DEFAULT_APP_ID);
    cfg->app_name    = thing_model_sample_getenv("THING_MODEL_APP_NAME",    SAMPLE_DEFAULT_APP_NAME);
    cfg->app_key     = thing_model_sample_getenv("THING_MODEL_APP_KEY",     SAMPLE_DEFAULT_APP_KEY);
    cfg->app_license = thing_model_sample_getenv("THING_MODEL_APP_LICENSE", SAMPLE_DEFAULT_APP_LICENSE);
    cfg->device_sn   = thing_model_sample_getenv("THING_MODEL_DEVICE_SN",   SAMPLE_DEFAULT_DEVICE_SN);

    /* ── broker ── */
    cfg->broker_address  = thing_model_sample_getenv("THING_MODEL_BROKER",  SAMPLE_DEFAULT_BROKER);
    cfg->broker_port_tls = 8883;
    cfg->broker_port     = 1883;
    cfg->ca_cert_path    = thing_model_sample_getenv("THING_MODEL_CA_CERT_PATH", SAMPLE_DEFAULT_CA_CERT);

    /* topic_style left at 0 = V2 (default).
     * transport_vtable / crypto_vtable left NULL = default mosquitto + openssl
     * (see the runtime documentation for replacing the default implementation). */

    /* ── Auto-reconnect: after disconnect, exponential backoff 1..30s in the background; on successful reconnect, auto-resubscribe to subscribed topics ── */
    cfg->auto_reconnect          = true;
    cfg->reconnect_delay_min_sec = 1;
    cfg->reconnect_delay_max_sec = 30;
}

/* Print a masked secret: placeholder shows as not set, otherwise mask keeping first 3 + last 2 chars. */
static void print_secret(const char *label, const char *v)
{
    if (!v || !*v) {
        printf("  %-12s = (empty)\n", label);
    } else if (strncmp(v, "YOUR_", 5) == 0) {
        printf("  %-12s = %s  (placeholder — configure it in sample_config_local.h or via environment variables)\n", label, v);
    } else {
        size_t n = strlen(v);
        if (n <= 6) printf("  %-12s = ***\n", label);
        else        printf("  %-12s = %.3s***%.2s (len=%zu)\n", label, v, v + n - 2, n);
    }
}

void thing_model_sample_config_print(const ThingModelClientSession_Config_t *cfg)
{
    if (!cfg) return;
    printf("[sample] session config:\n");
    printf("  %-12s = %s:%u (tls %u)\n", "broker", cfg->broker_address,
           cfg->broker_port, cfg->broker_port_tls);
    printf("  %-12s = %s\n", "app_id", cfg->app_id ? cfg->app_id : "(null)");
    printf("  %-12s = %s\n", "device_sn", cfg->device_sn ? cfg->device_sn : "(null)");
    print_secret("app_key",     cfg->app_key);
    print_secret("app_license", cfg->app_license);
}

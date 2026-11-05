/**
 * @file    sample_config_rtos.c
 * @brief   RTOS-side sample_config implementation — same interface names as the host-side sample_config.c,
 *          but internally does not depend on getenv / environment variables; uses hardcoded values from sample_config_local.h directly.
 *
 *   Exposes exactly the same as samples/sample_things_model/common/sample_config.h:
 *     - thing_model_sample_config_load(cfg)
 *     - thing_model_sample_config_print(cfg)
 *     - thing_model_sample_getenv(key, def)     RTOS returns def directly
 *
 *   This way the launcher (sample_main.c) calls thing_model_sample_config_load() to get cfg,
 *   host goes through sample_config.c (getenv + environment variables), RTOS goes through this file (hardcoded + injected
 *   transport/crypto vtable). CMakeLists selects which one is compiled, deciding which implementation goes into the elf.
 *
 *   Key differences from host:
 *     - does not read environment variables (the MCU has no getenv)
 *     - does not use __has_include "sample_config_local.h" soft import — RTOS #includes it directly as a hard dependency
 *       (sample_config_local.h is in the same directory as this file)
 *     - explicitly injects cfg->transport_vtable / crypto_vtable
 *       (the runtime weak factory cannot find mosquitto/openssl on RTOS)
 *
 *   The macro names in sample_config_local.h (SAMPLE_APP_ID) differ from the host-side example
 *   (SAMPLE_DEFAULT_APP_ID); both are local credentials and are independent.
 */
#include "sample_config.h"           /* host interface header, provides ThingModelClientSession_Config_t */
#include "sample_config_local.h"

#include "thing_model_client/thing_model_client_transport.h"
#include "thing_model_client/thing_model_client_crypto.h"

#include <string.h>
#include <stdio.h>

const char *thing_model_sample_getenv(const char *key, const char *def)
{
    (void)key;   /* MCU has no getenv, return def directly */
    return def;
}

void thing_model_sample_config_load(ThingModelClientSession_Config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* access credentials (sample_config_local.h, same directory as this file, not included in the public release package) */
    cfg->app_id      = SAMPLE_APP_ID;
    cfg->app_name    = SAMPLE_APP_NAME;
    cfg->app_key     = SAMPLE_APP_KEY;
    cfg->app_license = SAMPLE_APP_LICENSE;
    cfg->device_sn   = SAMPLE_DEVICE_SN;

    /* Broker */
    cfg->broker_address  = SAMPLE_BROKER_ADDR;
    cfg->broker_port_tls = SAMPLE_BROKER_PORT_TLS;
    cfg->broker_port     = SAMPLE_BROKER_PORT;
    cfg->ca_cert_path    = NULL;   /* transport_lwip goes through weak thing_model_lwip_ca_cert() */

    /* MQTT */
    cfg->keepalive_sec      = 60;
    cfg->default_timeout_ms = 5000;
    cfg->topic_style        = THING_MODEL_TOPIC_STYLE_V2;

    /* auto reconnect: 1..30s exponential backoff */
    cfg->auto_reconnect          = true;
    cfg->reconnect_delay_min_sec = 1;
    cfg->reconnect_delay_max_sec = 30;

    /* RTOS platform explicitly injects the vtable — the runtime weak fallback only points to mosquitto/openssl,
     * which embedded does not have, so it must be specified explicitly. The host-side sample_config.c does not do this (uses defaults). */
    cfg->transport_vtable = ThingModelTransport_LwipMbedtls();
    cfg->crypto_vtable    = ThingModelCrypto_Mbedtls();
}

void thing_model_sample_config_print(const ThingModelClientSession_Config_t *cfg)
{
    if (!cfg) return;
    printf("[sample] session config:\r\n");
    printf("  broker      = %s:%u (tls %u)\r\n",
           cfg->broker_address, cfg->broker_port, cfg->broker_port_tls);
    printf("  app_id      = %s\r\n", cfg->app_id);
    printf("  device_sn   = %s\r\n", cfg->device_sn);
    printf("  app_key     = %.4s***  (len=%u)\r\n",
           cfg->app_key, (unsigned)strlen(cfg->app_key));
    printf("  app_license = ***  (len=%u)\r\n",
           (unsigned)strlen(cfg->app_license));
    printf("  topic_style = V%u, auto_reconnect=%d (%u..%u s)\r\n",
           cfg->topic_style == THING_MODEL_TOPIC_STYLE_V2 ? 2 : 1,
           (int)cfg->auto_reconnect,
           cfg->reconnect_delay_min_sec, cfg->reconnect_delay_max_sec);
}

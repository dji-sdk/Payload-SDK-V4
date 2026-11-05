/*
 * sample_session.c — sample session facade implementation, see sample_session.h.
 *
 * The single session-lifecycle assembly point: the start/stop order of device-level common concerns is centralized here,
 * so each sample no longer maintains it individually. Adding a device-level cross-cutting piece = one line in each of the two functions below.
 */
#include "sample_session.h"
#include "sample_device_report.h"   /* shared device report */
#include "bind_monitor.h"           /* shared bind-state listener */
#include "sample_aircraft_info.h"   /* shared aircraft-info listener (OSD, cross-cap device-level) */

#include <string.h>

ThingModelClientSession_t *sample_session_open(const ThingModelClientSession_Config_t *cfg)
{
    if (!cfg) return NULL;
    ThingModelClientSession_t *s = ThingModelClientSession_Connect(cfg);
    if (!s) return NULL;

    /* ── Device-level common bootstrap (add new concerns here) ── */
    sample_device_report_start(s, cfg->device_sn, NULL);  /* periodically report device_info */
    bind_monitor_start(s);                                /* listen for bind state (cross-cap device-level) */
    sample_aircraft_info_start(s);                        /* listen for aircraft info (OSD, cross-cap device-level; internally gated by the toggle) */
    return s;
}

void sample_session_close(ThingModelClientSession_t *s)
{
    if (!s) return;
    /* ── Teardown in reverse order of open ── */
    sample_aircraft_info_stop();
    bind_monitor_stop();
    sample_device_report_stop();
    ThingModelClientSession_Disconnect(s);
}

ThingModelClientSession_t *sample_session_acquire(const SampleCtx *ctx,
                                                  const ThingModelClientSession_Config_t *cfg,
                                                  bool *owned)
{
    /* Reuse the launcher's shared session only when the caller's identity matches it (no --sn override):
     * a different device_sn means a different MQTT client_id, which needs its own connection. */
    if (ctx && ctx->session && ctx->cfg && cfg &&
        cfg->device_sn && ctx->cfg->device_sn &&
        strcmp(cfg->device_sn, ctx->cfg->device_sn) == 0) {
        if (owned) *owned = false;
        return ctx->session;
    }
    if (owned) *owned = true;
    return sample_session_open(cfg);
}

void sample_session_release(ThingModelClientSession_t *s, bool owned)
{
    if (owned) sample_session_close(s);
}

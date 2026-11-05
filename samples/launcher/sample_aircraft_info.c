/*
 * sample_aircraft_info.c — shared aircraft-info listener implementation, see sample_aircraft_info.h.
 *
 * The T_AircraftInfoPush struct and its decoder are declared in aircraft_info/aircraft_info_json.h;
 * this file subscribes, fans out, and gates on a toggle.
 */
#include "sample_aircraft_info.h"

#include "aircraft_info/aircraft_info_json.h"  /* provides: T_AircraftInfoPush / AircraftInfoJson_DecodeAircraftInfoPush / AircraftInfo_FreeAircraftInfoPush */
#include "sample_config.h"                     /* thing_model_sample_getenv (reads the toggle env var, MCU-safe fallback) */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client_log.h"

#include <stdio.h>
#include <string.h>

/* Toggle default value (compile-time): fallback when MCU has no getenv; can also -DSAMPLE_AIRCRAFT_INFO_DEFAULT=0 to disable by default at build time. */
#ifndef SAMPLE_AIRCRAFT_INFO_DEFAULT
#  define SAMPLE_AIRCRAFT_INFO_DEFAULT 1
#endif

/* Singleton: one session, one listener for the whole process. */
static ThingModelClientSession_t *s_session   = NULL;
static int32_t                    s_listener  = -1;
static volatile int               s_got_push  = 0;
static volatile int               s_enabled   = -1;  /* -1=not seeded, 0=off, 1=on; volatile: menu thread toggles it, transport thread reads it in the callback */
static uint64_t                   s_last_print_ms = 0; /* last print time (OSAL monotonic ms); throttle steady-state pushes to 1/3s */

/* Header-independent case-insensitive compare (avoids depending on strings.h, MCU-friendly). */
static bool ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return false;
    }
    return *a == *b;
}

/* env value truthiness: "0"/"off"/"false"/"no"/"n"/"f" -> off; any other non-empty -> on. */
static bool truthy(const char *v)
{
    if (!v || !*v) return true;
    if (!strcmp(v, "0") || ieq(v, "off") || ieq(v, "false") ||
        ieq(v, "no") || ieq(v, "n") || ieq(v, "f"))
        return false;
    return true;
}

/* Lazily seed the toggle's initial value: env THING_MODEL_AIRCRAFT_INFO > compile-time macro. */
static void ensure_seeded(void)
{
    if (s_enabled >= 0) return;
    const char *v = thing_model_sample_getenv("THING_MODEL_AIRCRAFT_INFO", NULL);
    if (v) s_enabled = truthy(v) ? 1 : 0;
    else   s_enabled = SAMPLE_AIRCRAFT_INFO_DEFAULT ? 1 : 0;
}

/* Whether the payload contains substring needle (payload is not NUL-terminated, searched within len). */
static bool payload_contains(const char *hay, size_t hay_len, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0 || hay_len < nl) return false;
    for (size_t i = 0; i + nl <= hay_len; i++)
        if (memcmp(hay + i, needle, nl) == 0) return true;
    return false;
}

/* session listener: fanned every PUBLISH received (transport thread, holds listener_lock).
 * Only recognizes aircraft_info_push on the osd channel; everything else returns fast, no JSON parse (to avoid dragging the hot push path).
 * Forbidden: this callback must not call sync API / RemoveListener (it runs on the transport thread holding the listener lock). */
static void on_session_msg(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    /* Runtime toggle gate: on the shared session the listener is added ONCE at startup and never
     * re-created per sample, so the menu's on/off must be honored HERE — otherwise toggling off
     * only flips the flag while pushes keep printing (i.e. "can't turn it off"). Stays subscribed
     * at the broker, just surfaces nothing while off; flipping back on resumes with no reconnect. */
    if (!s_enabled) return;
    /* First gate: only look at the osd channel (V2 "osd"); second gate: bare-scan the method substring, decode only on hit. */
    if (!strstr(topic, "osd"))                                return;
    if (!payload_contains(payload, len, "aircraft_info_push")) return;

    /* The decoder automatically drills into the outer "data", consuming the whole wire payload directly.
     * The payload is guaranteed NUL-terminated by the transport (the mosq adapter layer already zero-padded), so pass it directly. */
    T_AircraftInfoPush a;
    AircraftInfo_InitAircraftInfoPush(&a);
    if (AircraftInfoJson_DecodeAircraftInfoPush(payload, &a) == THING_MODEL_RC_OK) {
        /* Print the first push immediately, then throttle to at most once every 3s (avoid OSD flooding).
         * Time source is the OSAL monotonic clock (POSIX gettimeofday / RTOS xTaskGetTickCount) — no libc time().
         * Platforms without time_ms return 0 -> print every frame. */
        const T_ThingModelOsal *osal = ThingModelClientSession_GetOsal(s_session);
        uint64_t now_ms = (osal && osal->time_ms) ? osal->time_ms() : 0;
        if (!s_got_push || now_ms == 0 || now_ms - s_last_print_ms >= 3000) {
            s_got_push      = 1;
            s_last_print_ms = now_ms;
            LOG_INFO("[aircraft_info] type=%d firmware=%s App{lang=%d screen=%d} devices=%u",
                     a.aircraft_type,
                     a.aircraft_version ? a.aircraft_version : "-",
                     a.mobile_app_info.language,
                     a.mobile_app_info.screen_type,
                     (unsigned)a.devices_info_count);
            for (size_t i = 0; i < a.devices_info_count; i++) {
                const T_AircraftDeviceInfo *d = &a.devices_info[i];
                LOG_INFO("[aircraft_info]   dev[%u]: id=%s name=%s sn=%s fw=%s type=%d",
                         (unsigned)i,
                         d->device_id        ? d->device_id        : "-",
                         d->device_name      ? d->device_name      : "-",
                         d->serial_number    ? d->serial_number    : "-",
                         d->firmware_version ? d->firmware_version : "-",
                         d->device_type);
            }
        }
    }
    AircraftInfo_FreeAircraftInfoPush(&a);
}

int sample_aircraft_info_start(ThingModelClientSession_t *session)
{
    ensure_seeded();
    if (!s_enabled) return 0;           /* toggle off: don't subscribe (return 0, facade calls blindly) */
    if (s_listener >= 0) return 0;      /* already running, idempotent */
    if (!session) return -1;

    s_session  = session;
    s_listener = ThingModelClientSession_AddListener(session, on_session_msg, NULL);
    if (s_listener < 0) { s_session = NULL; return -1; }

    /* Self-subscribe to osd: guarantees "receive on connect" regardless of which cap runs (broker-side subscription is idempotent).
     * Uses persistent Subscribe; on auto_reconnect, re-subscribes automatically after a disconnect. */
    const char *sn = ThingModelClientSession_GetSn(session);
    char topic[256];
    snprintf(topic, sizeof(topic), "thing/product/%s/osd", sn ? sn : "");
    ThingModelClientSession_Subscribe(session, topic, 0);

    LOG_INFO("[aircraft_info] aircraft-info listener started (listener=%d, subscribed %s)", s_listener, topic);
    return 0;
}

void sample_aircraft_info_stop(void)
{
    if (s_listener < 0) return;
    ThingModelClientSession_RemoveListener(s_session, s_listener);
    s_listener = -1;
    s_session  = NULL;
}

void sample_aircraft_info_set_enabled(bool on)
{
    s_enabled = on ? 1 : 0;             /* explicit set counts as seeded */
}

bool sample_aircraft_info_enabled(void)
{
    ensure_seeded();
    return s_enabled != 0;
}

bool sample_aircraft_info_got_push(void) { return s_got_push != 0; }

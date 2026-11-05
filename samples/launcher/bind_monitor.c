/*
 * bind_monitor.c — shared device bind-state listener implementation, see bind_monitor.h.
 *
 * The T_BindRequired struct and its decoder are declared in bind/bind_json.h.
 * This file only subscribes and fans out messages; protocol decoding lives in the generated decoder.
 */
#include "bind_monitor.h"

#include "bind/bind_json.h"     /* provides: T_BindRequired / BindJson_DecodeBindRequired / Bind_FreeBindRequired */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client_log.h"

#include <stdio.h>
#include <string.h>

/* Singleton: one session, one listener for the whole process. */
static ThingModelClientSession_t *s_session       = NULL;
static int32_t                    s_listener       = -1;
static volatile int               s_bind_state     = BIND_STATE_IDLE;
static volatile int               s_got_push       = 0;

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
 * Only recognizes bind_required; everything else returns fast, no JSON parse (to avoid dragging the hot push path).
 * Forbidden: this callback must not call sync API / RemoveListener (it runs on the transport thread holding the listener lock). */
static void on_session_msg(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    /* First gate: only look at the event channel (V2 "events" / V1 "event" both contain substring "event"); second gate: bare-scan the substring, decode only on hit. */
    if (!strstr(topic, "event"))                          return;
    if (!payload_contains(payload, len, "bind_required")) return;

    /* The decoder automatically drills into the outer "data", consuming the whole wire payload directly. */
    T_BindRequired b;
    Bind_InitBindRequired(&b);
    /* payload is not NUL-terminated, but Decode parses it as a string internally; the payload the listener receives is
     * guaranteed NUL-terminated by the transport (the mosq adapter layer already zero-padded), so pass it directly. */
    if (BindJson_DecodeBindRequired(payload, &b) == THING_MODEL_RC_OK) {
        s_bind_state = (int)b.bind_state;
        s_got_push   = 1;
        LOG_INFO("[bind] device bind state: %d (%s) - %s",
                 (int)b.bind_state,
                 b.bind_state_desc ? b.bind_state_desc : "-",
                 b.message         ? b.message         : "-");
        if (b.bind_state != BIND_STATE_BOUND) {
            LOG_WARN("[bind] device not bound / binding not complete; the cloud may reject business requests; "
                     "please complete binding via DA2 and retry.");
        }
    }
    Bind_FreeBindRequired(&b);
}

int bind_monitor_start(ThingModelClientSession_t *session)
{
    if (s_listener >= 0) return 0;      /* already running, idempotent */
    if (!session) return -1;

    s_session  = session;
    s_listener = ThingModelClientSession_AddListener(session, on_session_msg, NULL);
    if (s_listener < 0) { s_session = NULL; return -1; }

    /* Self-subscribe to events: guarantees "receive on connect" regardless of which cap runs (broker-side subscription is idempotent).
     * Uses persistent Subscribe; on auto_reconnect, re-subscribes automatically after a disconnect. */
    const char *sn = ThingModelClientSession_GetSn(session);
    char topic[256];
    snprintf(topic, sizeof(topic), "thing/product/%s/events", sn ? sn : "");
    ThingModelClientSession_Subscribe(session, topic, 0);

    LOG_INFO("[bind] bind-state listener started (listener=%d, subscribed %s)", s_listener, topic);
    return 0;
}

void bind_monitor_stop(void)
{
    if (s_listener < 0) return;
    ThingModelClientSession_RemoveListener(s_session, s_listener);
    s_listener = -1;
    s_session  = NULL;
}

bool bind_monitor_is_bound(void)
{
    if (!s_got_push) return true;       /* never pushed = conservatively allow */
    return s_bind_state == BIND_STATE_BOUND;
}

int  bind_monitor_state(void)  { return s_bind_state; }
bool bind_monitor_got_push(void) { return s_got_push != 0; }

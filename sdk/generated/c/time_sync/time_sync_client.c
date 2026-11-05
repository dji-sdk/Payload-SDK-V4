/**
 * @file    time_sync_client.c    (self-contained)
 *
 *   This file uses only the publish/subscribe/listener primitives from thing_model_client_session
 *   plus OSAL and cJSON helpers.
 *   pending table / push table / on_message dispatch / synchronous wait / asynchronous dispatch — all contained in this file.
 */
#include "time_sync_client.h"
#include "time_sync_topics.h"
#include "time_sync_json.h"
#include "common/common_json.h"
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client/thing_model_client_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* Local constants & utilities                                    */
/* ============================================================ */

static const char *KIND_SUFFIX_V1[THING_MODEL_TOPIC_KIND_MAX] = {
    "service", "service_reply",
    "properties/get", "properties/get_reply",
    "properties/set", "properties/set_reply",
    "event", "osd", "state",
};

/* V2 topic suffixes: services / property/get / events / osd / state.
 * Note the index must align with E_ThingModelTopicKind: EVENT=6 / OSD=7 / STATE=8.
 * Slot 7 is OSD, must be filled with "osd" (the aircraft publishes high-frequency OSD pushes to thing/product/<sn>/osd). */
static const char *KIND_SUFFIX_V2[THING_MODEL_TOPIC_KIND_MAX] = {
    "services", "services_reply",
    "property/get", "property/get_reply",
    "property/set", "property/set_reply",
    "events", "osd", "state",
};

static void build_topic(char *out, size_t cap, E_ThingModelTopicStyle style,
                        const char *sn, E_ThingModelTopicKind kind)
{
    if (style == THING_MODEL_TOPIC_STYLE_V2)
        /* V2 fixed owner=product, without the cap segment, and no leading / before thing */
        snprintf(out, cap, "thing/product/%s/%s",
                 sn, KIND_SUFFIX_V2[kind]);
    else
        snprintf(out, cap, "/thing/%s/%s/%s/%s",
                 TIME_SYNC_TOPIC_OWNER, sn,
                 TIME_SYNC_TOPIC_CAP_NAME, KIND_SUFFIX_V1[kind]);
}

static void build_method(char *out, size_t cap, E_ThingModelTopicStyle style,
                         const char *base)
{
    if (style == THING_MODEL_TOPIC_STYLE_V2)
        snprintf(out, cap, "%s_%s", TIME_SYNC_TOPIC_CAP_NAME, base);
    else
        snprintf(out, cap, "%s", base);
}

char *TimeSync_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind)
{
    char buf[256];
    build_topic(buf, sizeof(buf), style, sn ? sn : "", kind);
    return ThingModelMem_Strdup(buf);
}

const char *TimeSync_GeneratorVersion(void) { return TIME_SYNC_GENERATOR_VERSION; }

/* ============================================================ */
/* pending / push tables                                          */
/* ============================================================ */

typedef enum { PEND_SYNC, PEND_ASYNC, PEND_ASYNC_TYPED } EPendKind;

/* Used for PEND_ASYNC_TYPED — collapses the decode of a "specific return type" + the user cb call into
 * two function pointers, so the dispatcher handles all typed RPCs in one piece of code, with no per-rpc dispatch. */
typedef T_ThingModelReturnCode (*_TypedDecoder)(const char *json, void *out_reply);
typedef void                   (*_TypedAsyncCb)(void *user, T_ThingModelReturnCode rc, const void *typed_reply);

typedef struct PendingEntry {
    struct PendingEntry *next;
    uint32_t  seq;
    char      reply_topic[256];
    char      expect_method[96];     /* nullable (property reply carries no method) */
    EPendKind kind;
    bool      fired;

    /* sync path */
    void     *event;                 /* OSAL event */
    char     *sync_buf;
    size_t    sync_buf_cap;
    size_t    sync_buf_len;

    /* async (CommonReply) path */
    T_ThingModelAsyncReplyCb cb;
    void             *cb_user;

    /* async (typed reply) path — only used by PEND_ASYNC_TYPED */
    _TypedAsyncCb    typed_cb;
    _TypedDecoder    typed_decoder;
    size_t           typed_reply_size;
} PendingEntry_t;

typedef struct PushEntry {
    struct PushEntry *next;
    int32_t   handle;
    char      topic[256];
    char      expect_method[96];     /* nullable, used by V2 */
    void    (*cb)(void *user, const char *payload, size_t len);
    void     *user;
} PushEntry_t;

struct TimeSync_Client {
    ThingModelClientSession_t *session;
    E_ThingModelTopicStyle     style;
    int32_t             listener_handle;
    char                topic[THING_MODEL_TOPIC_KIND_MAX][256];

    void               *lock;
    PendingEntry_t     *pending;
    PushEntry_t        *pushes;
    int32_t             next_push_handle;
};

/* ============================================================ */
/* on_message dispatch                                            */
/* ============================================================ */

static void on_session_message(void *user, const char *topic,
                               const char *payload, size_t len)
{
    TimeSync_Client_t *c = user;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);

    /* Shallow parse once: get seq + method (set to empty if either is missing) */
    uint32_t    in_seq = 0;
    const char *in_method = NULL;
    ThingModelJsonObj_t *root = ThingModelJson_ParseN(payload, len);
    if (root) {
        in_seq    = (uint32_t)ThingModelJson_LookupInt(root, "seq", 0);
        in_method = ThingModelJson_LookupString(root, "method", NULL);
    }

    /* 1) pending: match by (topic, seq, [method]).
     *    sync completes the memcpy+fired+signal inside the lock (avoids a race between the caller's
     *    event_destroy after timeout and the transport's signal/memcpy); async only calls the cb after
     *    releasing the lock (avoids self-deadlock from calling a user callback while holding the lock). */
    PendingEntry_t *hit = NULL;
    o->mutex_lock(c->lock);
    PendingEntry_t **pp = &c->pending;
    while (*pp) {
        PendingEntry_t *e = *pp;
        if (e->seq == in_seq && strcmp(e->reply_topic, topic) == 0
            && (!e->expect_method[0] || (in_method && strcmp(e->expect_method, in_method) == 0))) {
            *pp = e->next;
            hit = e;
            break;
        }
        pp = &e->next;
    }
    if (hit && hit->kind == PEND_SYNC) {
        /* SYNC: complete all delivery work inside the lock */
        if (hit->sync_buf && len < hit->sync_buf_cap) {
            memcpy(hit->sync_buf, payload, len);
            hit->sync_buf[len] = 0;
            hit->sync_buf_len  = len;
        }
        hit->fired = true;
        o->event_signal(hit->event);
        o->mutex_unlock(c->lock);
        if (root) ThingModelJson_Free(root);
        return;
    }
    o->mutex_unlock(c->lock);

    if (hit) {
        /* ASYNC: cb is called outside the lock to avoid self-deadlock */
        if (hit->kind == PEND_ASYNC_TYPED) {
            if (hit->typed_cb) {
                void *r = ThingModelMem_Calloc(1, hit->typed_reply_size);
                if (r) {
                    if (hit->typed_decoder) hit->typed_decoder(payload, r);
                    hit->typed_cb(hit->cb_user, THING_MODEL_RC_OK, r);
                    ThingModelMem_Free(r);
                } else {
                    hit->typed_cb(hit->cb_user, THING_MODEL_RC_OUT_OF_MEMORY, NULL);
                }
            }
        } else if (hit->cb) {
            T_ThingModelCommonReply r = {0};
            TimeSyncJson_DecodeCommonReply(payload, &r);
            hit->cb(hit->cb_user, THING_MODEL_RC_OK, &r);
        }
        ThingModelMem_Free(hit);
        if (root) ThingModelJson_Free(root);
        return;
    }

    /* 2) push: snapshot the matching (cb, user) pairs inside the lock, then call them after releasing it,
     *    to avoid calling a user callback while holding c->lock (a user callback calling a sync API again would self-deadlock). */
    typedef struct { void (*cb)(void *, const char *, size_t); void *user; } CbPair_t;
    enum { CB_STACK_CAP = 8 };
    CbPair_t  cb_stack[CB_STACK_CAP];
    CbPair_t *cb_buf = cb_stack;
    int       cb_cap = CB_STACK_CAP, cb_n = 0;

    o->mutex_lock(c->lock);
    for (PushEntry_t *p = c->pushes; p; p = p->next) {
        if (strcmp(p->topic, topic) != 0) continue;
        if (p->expect_method[0]
            && (!in_method || strcmp(p->expect_method, in_method) != 0)) continue;
        if (cb_n == cb_cap) {
            int new_cap = cb_cap * 2;
            CbPair_t *nb;
            if (cb_buf == cb_stack) {
                nb = (CbPair_t *)ThingModelMem_Alloc(new_cap * sizeof(CbPair_t));
                if (nb) memcpy(nb, cb_stack, cb_n * sizeof(CbPair_t));
            } else {
                /* OSAL has no realloc semantics, so alloc+copy+free manually */
                nb = (CbPair_t *)ThingModelMem_Alloc(new_cap * sizeof(CbPair_t));
                if (nb) {
                    memcpy(nb, cb_buf, cb_n * sizeof(CbPair_t));
                    ThingModelMem_Free(cb_buf);
                }
            }
            if (!nb) break;   /* on out-of-memory, degrade to partial callback of what was already snapshotted */
            cb_buf = nb; cb_cap = new_cap;
        }
        cb_buf[cb_n].cb   = p->cb;
        cb_buf[cb_n].user = p->user;
        cb_n++;
    }
    o->mutex_unlock(c->lock);

    for (int i = 0; i < cb_n; i++) cb_buf[i].cb(cb_buf[i].user, payload, len);
    if (cb_buf != cb_stack) ThingModelMem_Free(cb_buf);

    if (root) ThingModelJson_Free(root);
}

/* ============================================================ */
/* sync / async helpers                                           */
/* ============================================================ */

static T_ThingModelReturnCode call_sync(TimeSync_Client_t *c, E_ThingModelTopicKind pub_kind,
                                 E_ThingModelTopicKind reply_kind,
                                 const char *method,            /* NULL = no method validation */
                                 const char *payload_json,
                                 char *out_buf, size_t out_cap, uint32_t timeout_ms)
{
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);

    uint32_t seq = (uint32_t)0;
    /* seq is carried by payload_json itself; we parse it out to match */
    {
        ThingModelJsonObj_t *r = ThingModelJson_Parse(payload_json);
        if (r) { seq = (uint32_t)ThingModelJson_LookupInt(r, "seq", 0); ThingModelJson_Free(r); }
    }
    if (seq == 0) return THING_MODEL_RC_INVALID_ARG;

    PendingEntry_t e = {0};
    e.seq          = seq;
    e.kind         = PEND_SYNC;
    e.event        = o->event_create();
    e.sync_buf     = out_buf;
    e.sync_buf_cap = out_cap;
    snprintf(e.reply_topic, sizeof(e.reply_topic), "%s", c->topic[reply_kind]);
    if (method) snprintf(e.expect_method, sizeof(e.expect_method), "%s", method);

    o->mutex_lock(c->lock);
    e.next = c->pending;
    c->pending = &e;
    o->mutex_unlock(c->lock);

    T_ThingModelReturnCode rc = ThingModelClientSession_Publish(c->session, c->topic[pub_kind], payload_json, 0);
    if (THING_MODEL_IS_OK(rc)) {
        int wr = o->event_wait(e.event, timeout_ms);
        rc = (wr == 0 && e.fired) ? THING_MODEL_RC_OK : THING_MODEL_RC_TIMEOUT;
    }

    /* Detach entry: when the transport delivers a sync reply it has already, inside the lock, done memcpy+signal
     * and removed the entry from the list. We take the lock again to look for it in the list; once "not found"
     * it must have been delivered (e.fired must be true), at which point we correct rc back to OK; the lock's
     * serialization guarantees the transport won't touch the entry after we release the lock, so the
     * event_destroy below is safe. */
    o->mutex_lock(c->lock);
    bool still_in_list = false;
    PendingEntry_t **pp = &c->pending;
    while (*pp) {
        if (*pp == &e) { *pp = e.next; still_in_list = true; break; }
        pp = &(*pp)->next;
    }
    if (!still_in_list && e.fired) rc = THING_MODEL_RC_OK;
    o->mutex_unlock(c->lock);
    o->event_destroy(e.event);
    return rc;
}

static T_ThingModelReturnCode call_async(TimeSync_Client_t *c, E_ThingModelTopicKind pub_kind,
                                  E_ThingModelTopicKind reply_kind, const char *method,
                                  const char *payload_json,
                                  T_ThingModelAsyncReplyCb cb, void *user)
{
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);

    uint32_t seq = 0;
    {
        ThingModelJsonObj_t *r = ThingModelJson_Parse(payload_json);
        if (r) { seq = (uint32_t)ThingModelJson_LookupInt(r, "seq", 0); ThingModelJson_Free(r); }
    }
    if (seq == 0) return THING_MODEL_RC_INVALID_ARG;

    PendingEntry_t *e = ThingModelMem_Calloc(1, sizeof(*e));
    if (!e) return THING_MODEL_RC_OUT_OF_MEMORY;
    e->seq     = seq;
    e->kind    = PEND_ASYNC;
    e->cb      = cb;
    e->cb_user = user;
    snprintf(e->reply_topic, sizeof(e->reply_topic), "%s", c->topic[reply_kind]);
    if (method) snprintf(e->expect_method, sizeof(e->expect_method), "%s", method);

    o->mutex_lock(c->lock);
    e->next    = c->pending;
    c->pending = e;
    o->mutex_unlock(c->lock);

    T_ThingModelReturnCode rc = ThingModelClientSession_Publish(c->session, c->topic[pub_kind], payload_json, 0);
    if (!THING_MODEL_IS_OK(rc)) {
        o->mutex_lock(c->lock);
        PendingEntry_t **pp = &c->pending;
        while (*pp) { if (*pp == e) { *pp = e->next; break; } pp = &(*pp)->next; }
        o->mutex_unlock(c->lock);
        ThingModelMem_Free(e);
    }
    return rc;
}

/* call_async_typed — same as call_async, but the reply goes through the "bring-your-own decoder + typed cb" path
 * (PEND_ASYNC_TYPED). Used for SERVICE rpcs that don't return CommonResponse. */
static T_ThingModelReturnCode call_async_typed(TimeSync_Client_t *c, E_ThingModelTopicKind pub_kind,
                                  E_ThingModelTopicKind reply_kind, const char *method,
                                  const char *payload_json,
                                  _TypedAsyncCb typed_cb, void *user,
                                  _TypedDecoder decoder, size_t reply_size)
{
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);

    uint32_t seq = 0;
    {
        ThingModelJsonObj_t *r = ThingModelJson_Parse(payload_json);
        if (r) { seq = (uint32_t)ThingModelJson_LookupInt(r, "seq", 0); ThingModelJson_Free(r); }
    }
    if (seq == 0) return THING_MODEL_RC_INVALID_ARG;

    PendingEntry_t *e = ThingModelMem_Calloc(1, sizeof(*e));
    if (!e) return THING_MODEL_RC_OUT_OF_MEMORY;
    e->seq              = seq;
    e->kind             = PEND_ASYNC_TYPED;
    e->typed_cb         = typed_cb;
    e->cb_user          = user;
    e->typed_decoder    = decoder;
    e->typed_reply_size = reply_size;
    snprintf(e->reply_topic, sizeof(e->reply_topic), "%s", c->topic[reply_kind]);
    if (method) snprintf(e->expect_method, sizeof(e->expect_method), "%s", method);

    o->mutex_lock(c->lock);
    e->next    = c->pending;
    c->pending = e;
    o->mutex_unlock(c->lock);

    T_ThingModelReturnCode rc = ThingModelClientSession_Publish(c->session, c->topic[pub_kind], payload_json, 0);
    if (!THING_MODEL_IS_OK(rc)) {
        o->mutex_lock(c->lock);
        PendingEntry_t **pp = &c->pending;
        while (*pp) { if (*pp == e) { *pp = e->next; break; } pp = &(*pp)->next; }
        o->mutex_unlock(c->lock);
        ThingModelMem_Free(e);
    }
    return rc;
}

/* ============================================================ */
/* Create / Destroy                                              */
/* ============================================================ */

TimeSync_Client_t *TimeSync_CreateClient(ThingModelClientSession_t *session)
{
    if (!session) return NULL;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(session);
    TimeSync_Client_t *c = ThingModelMem_Calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->session = session;
    c->style   = ThingModelClientSession_GetTopicStyle(session);
    c->lock    = o->mutex_create();
    const char *sn = ThingModelClientSession_GetSn(session);
    for (int k = 0; k < THING_MODEL_TOPIC_KIND_MAX; k++)
        build_topic(c->topic[k], sizeof(c->topic[k]), c->style, sn, (E_ThingModelTopicKind)k);

    c->listener_handle = ThingModelClientSession_AddListener(session, on_session_message, c);

    ThingModelClientSession_Subscribe(session, c->topic[THING_MODEL_TOPIC_SERVICE_REPLY],  0);
    ThingModelClientSession_Subscribe(session, c->topic[THING_MODEL_TOPIC_PROP_GET_REPLY], 0);
    ThingModelClientSession_Subscribe(session, c->topic[THING_MODEL_TOPIC_PROP_SET_REPLY], 0);
    return c;
}

void TimeSync_DestroyClient(TimeSync_Client_t *c)
{
    /* Thread model / call constraints:
     *   - Push callbacks (registered by Camera_On<Event>) run on the transport thread; they do not hold c->lock.
     *     => Calling a sync API (Camera_Set*) inside a user callback will not self-deadlock, but will wait until
     *        timeout, because you are occupying the transport thread and it cannot dispatch this sync's reply.
     *        Use async instead, or issue the sync call from your own thread.
     *   - Do not call DestroyClient from inside a push callback; the transport thread will self-deadlock on
     *     session.listener_lock. Call it from the application main thread.
     */
    if (!c) return;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);
    ThingModelClientSession_RemoveListener(c->session, c->listener_handle);
    ThingModelClientSession_Unsubscribe(c->session, c->topic[THING_MODEL_TOPIC_SERVICE_REPLY]);
    ThingModelClientSession_Unsubscribe(c->session, c->topic[THING_MODEL_TOPIC_PROP_GET_REPLY]);
    ThingModelClientSession_Unsubscribe(c->session, c->topic[THING_MODEL_TOPIC_PROP_SET_REPLY]);
    /* Free leftover async pending (sync is managed by the caller) */
    PendingEntry_t *p = c->pending;
    while (p) { PendingEntry_t *n = p->next; if (p->kind != PEND_SYNC) ThingModelMem_Free(p); p = n; }
    /* Free push */
    PushEntry_t *pe = c->pushes;
    while (pe) { PushEntry_t *n = pe->next; ThingModelMem_Free(pe); pe = n; }
    if (c->lock) o->mutex_destroy(c->lock);
    ThingModelMem_Free(c);
}

/* ============================================================ */
/* SERVICE                                                       */
/* ============================================================ */

/* sync — CommonResponse return (only cares about result/result_message) */
T_ThingModelReturnCode TimeSync_SetSignalMode(TimeSync_Client_t *c, const T_TimeSyncSetSignalModeRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms)
{
    char method[128];
    build_method(method, sizeof(method), c->style, TIME_SYNC_METHOD_SET_SIGNAL_MODE);
    char *data = req ? TimeSyncJson_EncodeTimeSyncSetSignalModeRequest(req) : NULL;
    char *payload = TimeSyncJson_EncodeServiceRequest(ThingModelClientSession_NextSeq(c->session), method, data);
    if (data) ThingModelMem_Free(data);
    if (!payload) return THING_MODEL_RC_OUT_OF_MEMORY;
    char reply[2048];
    T_ThingModelReturnCode rc = call_sync(c, THING_MODEL_TOPIC_SERVICE, THING_MODEL_TOPIC_SERVICE_REPLY,
                                   method, payload, reply, sizeof(reply), timeout_ms);
    if (THING_MODEL_IS_OK(rc) && resp) TimeSyncJson_DecodeCommonReply(reply, resp);
    ThingModelMem_Free(payload);
    return rc;
}

T_ThingModelReturnCode TimeSync_SetSignalModeAsync(TimeSync_Client_t *c, const T_TimeSyncSetSignalModeRequest *req, T_ThingModelAsyncReplyCb cb, void *user)
{
    char method[128];
    build_method(method, sizeof(method), c->style, TIME_SYNC_METHOD_SET_SIGNAL_MODE);
    char *data = req ? TimeSyncJson_EncodeTimeSyncSetSignalModeRequest(req) : NULL;
    char *payload = TimeSyncJson_EncodeServiceRequest(ThingModelClientSession_NextSeq(c->session), method, data);
    if (data) ThingModelMem_Free(data);
    if (!payload) return THING_MODEL_RC_OUT_OF_MEMORY;
    T_ThingModelReturnCode rc = call_async(c, THING_MODEL_TOPIC_SERVICE, THING_MODEL_TOPIC_SERVICE_REPLY,
                                    method, payload, cb, user);
    ThingModelMem_Free(payload);
    return rc;
}

char *TimeSync_BuildSetSignalModePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_TimeSyncSetSignalModeRequest *req)
{
    char method[128];
    build_method(method, sizeof(method), style, TIME_SYNC_METHOD_SET_SIGNAL_MODE);
    char *data = req ? TimeSyncJson_EncodeTimeSyncSetSignalModeRequest(req) : NULL;
    char *payload = TimeSyncJson_EncodeServiceRequest(seq, method, data);
    if (data) ThingModelMem_Free(data);
    return payload;
}

/* ============================================================ */
/* PROPERTIES (whole group + field convenience)                  */
/* ============================================================ */

/* ============================================================ */
/* PUSH subscription (EVENT / OSD / STATE)                        */
/* ============================================================ */

typedef struct { TimeSync_Sync_Cb cb; void *user; } Sync_Ctx_t;

static void on_sync(void *user, const char *raw, size_t len)
{
    (void)len;
    Sync_Ctx_t *ctx = user;
    T_TimeSyncTimeSyncPush p;
    if (TimeSyncJson_DecodeTimeSyncTimeSyncPush(raw, &p) == THING_MODEL_RC_OK && ctx->cb)
        ctx->cb(ctx->user, &p);
}

T_ThingModelReturnCode TimeSync_OnSync(TimeSync_Client_t *c, TimeSync_Sync_Cb cb, void *user)
{
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(c->session);
    Sync_Ctx_t *ctx = ThingModelMem_Calloc(1, sizeof(*ctx));
    if (!ctx) return THING_MODEL_RC_OUT_OF_MEMORY;
    ctx->cb = cb; ctx->user = user;

    PushEntry_t *p = ThingModelMem_Calloc(1, sizeof(*p));
    if (!p) { ThingModelMem_Free(ctx); return THING_MODEL_RC_OUT_OF_MEMORY; }
    snprintf(p->topic, sizeof(p->topic), "%s", c->topic[THING_MODEL_TOPIC_EVENT]);
    if (c->style == THING_MODEL_TOPIC_STYLE_V2) {
        build_method(p->expect_method, sizeof(p->expect_method),
                     c->style, TIME_SYNC_METHOD_SYNC);
    }
    p->cb   = on_sync;
    p->user = ctx;

    o->mutex_lock(c->lock);
    p->handle = ++c->next_push_handle;
    p->next   = c->pushes;
    c->pushes = p;
    o->mutex_unlock(c->lock);

    ThingModelClientSession_Subscribe(c->session, p->topic, 0);
    return THING_MODEL_RC_OK;
}

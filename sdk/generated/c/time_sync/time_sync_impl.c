/**
 * @file    time_sync_impl.c    (self-contained)
 *
 * Server-side implementation (impl) — the device side receives cloud→device calls and returns a reply.
 * Covers three categories:
 *   - SERVICE                — dispatched to a service handler by the method field
 *   - PROPERTIES / READONLY  — field-level handlers; GET/SET dispatched by the field names present in data
 * Uses only the publish/subscribe/listener primitives from thing_model_client_session plus OSAL
 * and cJSON helpers.
 */
#include "time_sync_impl.h"
#include "time_sync_topics.h"
#include "time_sync_json.h"
#include "common/common_json.h"
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client/thing_model_client_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TIME_SYNC_GENERATOR_VERSION
#define TIME_SYNC_GENERATOR_VERSION "0.3.0"
#endif

const char *TimeSyncImpl_GeneratorVersion(void) { return TIME_SYNC_GENERATOR_VERSION; }

/* ============================================================ */
/* topic construction                                            */
/* ============================================================ */

static void build_impl_topic(char *out, size_t cap, E_ThingModelTopicStyle style,
                             const char *sn, E_ThingModelTopicKind kind)
{
    /* Aligned with the client-side build_topic behavior (same cap and same style must produce the same topic, or the two sides won't match). */
    static const char *KIND_SUFFIX_V1[THING_MODEL_TOPIC_KIND_MAX] = {
        "service", "service_reply",
        "properties/get", "properties/get_reply",
        "properties/set", "properties/set_reply",
        "event", "osd", "state",
    };
    static const char *KIND_SUFFIX_V2[THING_MODEL_TOPIC_KIND_MAX] = {
        "services", "services_reply",
        "property/get", "property/get_reply",
        "property/set", "property/set_reply",
        "events", "osd", "state",
    };
    if (style == THING_MODEL_TOPIC_STYLE_V2)
        snprintf(out, cap, "thing/product/%s/%s", sn, KIND_SUFFIX_V2[kind]);
    else
        snprintf(out, cap, "/thing/%s/%s/%s/%s",
                 TIME_SYNC_TOPIC_OWNER, sn,
                 TIME_SYNC_TOPIC_CAP_NAME, KIND_SUFFIX_V1[kind]);
}

/* ============================================================ */
/* SERVICE handler table                                         */
/* ============================================================ */

typedef enum { HANDLER_COMMON, HANDLER_TYPED } EHandlerKind;

typedef T_ThingModelReturnCode (*_AnyCommonHandler)(void *impl, void *user, const void *req, T_ThingModelCommonReply *reply);
typedef T_ThingModelReturnCode (*_AnyTypedHandler) (void *impl, void *user, const void *req, void *reply);

typedef T_ThingModelReturnCode (*_ReqDecoder)  (const char *data_json, void *out_req);
typedef char *                 (*_ReplyEncoder)(const void *reply);   /* returns malloc'd JSON */
/* Frees the heap fields (string/bytes/repeated/nested) inside req / typed reply, provided by the
 * generated <Pfx>_Free<Msg>; after that, the business code still needs to free() the outer struct shell (managed in this file).
 * A CommonResponse reply has no heap fields, so reply_free is left NULL when kind=COMMON. */
typedef void                   (*_StructFree)  (void *p);

typedef struct SvcHandlerEntry {
    struct SvcHandlerEntry *next;
    char           method[96];            /* full V2 method, e.g. "flight_upload_rid_info" */
    EHandlerKind   kind;
    void          *handler;
    void          *user;
    _ReqDecoder    req_decoder;
    _ReplyEncoder  reply_encoder;         /* only used by TYPED */
    _StructFree    req_free;              /* frees heap fields inside the req struct */
    _StructFree    reply_free;            /* frees heap fields inside the typed reply struct; left NULL for COMMON */
    size_t         req_size;
    size_t         reply_size;            /* only used by TYPED */
} SvcHandlerEntry_t;

/* ============================================================ */
/* PROPERTY handler table                                        */
/* ============================================================ */
/* Field-level handlers. Each field has at most one GET handler + one SET handler.
 * Type erasure: the concrete type (int64/double/bool/string/enum) selects the branch via vtype,
 * and the user callback signature is cast back to the real type inside the generated per-field wrapper. */

typedef enum {
    PVT_INT,        /* int64_t */
    PVT_DOUBLE,     /* double  */
    PVT_BOOL,       /* bool    */
    PVT_STRING,     /* string  — getter writes char*+cap; setter reads const char * */
    PVT_ENUM,       /* enum handled as int */
} EPropValueType;

/* Handler signature per type (unified interface after type erasure).
 * The field-level registration function wraps the user's concrete-type handler into this signature. */
typedef T_ThingModelReturnCode (*_PropGetIntH)   (void *impl, void *user, int64_t *out_v);
typedef T_ThingModelReturnCode (*_PropGetDblH)   (void *impl, void *user, double  *out_v);
typedef T_ThingModelReturnCode (*_PropGetBoolH)  (void *impl, void *user, bool    *out_v);
typedef T_ThingModelReturnCode (*_PropGetStrH)   (void *impl, void *user, char *out_buf, size_t cap);
typedef T_ThingModelReturnCode (*_PropSetIntH)   (void *impl, void *user, int64_t v);
typedef T_ThingModelReturnCode (*_PropSetDblH)   (void *impl, void *user, double  v);
typedef T_ThingModelReturnCode (*_PropSetBoolH)  (void *impl, void *user, bool    v);
typedef T_ThingModelReturnCode (*_PropSetStrH)   (void *impl, void *user, const char *v);

typedef struct PropHandlerEntry {
    struct PropHandlerEntry *next;
    char            field[64];         /* JSON field name */
    EPropValueType  vtype;
    /* GET / SET either may be null (NULL if not registered). */
    void           *get_handler;
    void           *get_user;
    void           *set_handler;
    void           *set_user;
} PropHandlerEntry_t;

/* ============================================================ */
/* struct                                                        */
/* ============================================================ */

struct TimeSyncImpl {
    ThingModelClientSession_t *session;
    E_ThingModelTopicStyle     style;
    char                       device_id[64];
    char                       topic_services[256];
    char                       topic_services_reply[256];
    char                       topic_prop_get[256];
    char                       topic_prop_get_reply[256];
    char                       topic_prop_set[256];
    char                       topic_prop_set_reply[256];
    int32_t                    listener_handle;
    void                      *lock;
    SvcHandlerEntry_t         *svc_handlers;
    PropHandlerEntry_t        *prop_handlers;
};

/* ============================================================ */
/* utilities                                                     */
/* ============================================================ */

static char *json_obj_to_string(const ThingModelJsonObj_t *obj)
{
    if (obj) {
        char *s = ThingModelJson_PrintCompact(obj);
        if (s) return s;
    }
    char *empty = ThingModelMem_Alloc(3);
    if (empty) { empty[0] = '{'; empty[1] = '}'; empty[2] = 0; }
    return empty;
}

static char *encode_common_reply_data(const T_ThingModelCommonReply *r)
{
    ThingModelJsonObj_t *o = ThingModelJson_NewObject();
    if (!o) return NULL;
    ThingModelJson_AddInt   (o, "result",         r ? (int64_t)r->result : 0);
    ThingModelJson_AddString(o, "result_message", r ? r->result_message  : "");
    char *s = ThingModelJson_PrintCompact(o);
    ThingModelJson_Free(o);
    return s;
}

static char *encode_error_reply_data(T_ThingModelReturnCode rc)
{
    T_ThingModelCommonReply r = {0};
    r.result = (int32_t)rc;
    snprintf(r.result_message, sizeof(r.result_message), "impl error rc=%d", (int)rc);
    return encode_common_reply_data(&r);
}

/* ============================================================ */
/* SERVICE dispatch                                              */
/* ============================================================ */

static void dispatch_service(TimeSyncImpl_t *im, ThingModelJsonObj_t *root,
                             uint32_t in_seq, const char *in_method)
{
    SvcHandlerEntry_t snap = {0};
    bool matched = false;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(im->session);
    o->mutex_lock(im->lock);
    for (SvcHandlerEntry_t *e = im->svc_handlers; e; e = e->next) {
        if (strcmp(e->method, in_method) == 0) {
            snap = *e;
            matched = true;
            break;
        }
    }
    o->mutex_unlock(im->lock);
    if (!matched) return;  /* unregistered method — silently ignore */

    ThingModelJsonObj_t *data_obj  = ThingModelJson_GetItem(root, "data");
    char                *data_json = json_obj_to_string(data_obj);

    size_t reply_sz = (snap.kind == HANDLER_TYPED) ? snap.reply_size : sizeof(T_ThingModelCommonReply);
    void  *req   = ThingModelMem_Calloc(1, snap.req_size);
    void  *reply = ThingModelMem_Calloc(1, reply_sz);
    T_ThingModelReturnCode rc = THING_MODEL_RC_OK;
    if (!req || !reply) {
        rc = THING_MODEL_RC_OUT_OF_MEMORY;
    } else {
        if (snap.req_decoder) snap.req_decoder(data_json, req);
        if (snap.kind == HANDLER_COMMON) {
            _AnyCommonHandler h = (_AnyCommonHandler)snap.handler;
            rc = h(im, snap.user, req, (T_ThingModelCommonReply *)reply);
        } else {
            _AnyTypedHandler h = (_AnyTypedHandler)snap.handler;
            rc = h(im, snap.user, req, reply);
        }
    }

    char *reply_data = NULL;
    if (THING_MODEL_IS_OK(rc)) {
        if (snap.kind == HANDLER_COMMON) {
            reply_data = encode_common_reply_data((T_ThingModelCommonReply *)reply);
        } else if (snap.reply_encoder) {
            reply_data = snap.reply_encoder(reply);
        }
    }
    if (!reply_data) {
        reply_data = encode_error_reply_data(THING_MODEL_IS_OK(rc) ? THING_MODEL_RC_OUT_OF_MEMORY : rc);
    }

    char *wire = TimeSyncJson_EncodeServiceRequest(in_seq, in_method, reply_data);
    if (wire) {
        ThingModelClientSession_Publish(im->session, im->topic_services_reply, wire, 0);
        ThingModelMem_Free(wire);
    }
    ThingModelMem_Free(reply_data);
    /* Free the heap fields (string/bytes/repeated/nested) inside req / typed reply,
     * provided by the generated <Pfx>_Free<Msg>; then ThingModelMem_Free() the shell. */
    if (req   && snap.req_free)   snap.req_free(req);
    if (reply && snap.reply_free) snap.reply_free(reply);
    ThingModelMem_Free(req);
    ThingModelMem_Free(reply);
    ThingModelMem_Free(data_json);
}

/* ============================================================ */
/* PROPERTY GET dispatch                                         */
/* ============================================================ */
/* wire request: {"seq":N,"data":["field_a","field_b",...]}  (array of field names)
 * wire reply:   {"seq":N,"data":{"field_a":<value>, "field_b":<value>, ...}}
 *               unregistered fields are omitted; for a registered field whose handler returns non-OK, the value = 0/false/"". */
static void dispatch_prop_get(TimeSyncImpl_t *im, ThingModelJsonObj_t *root, uint32_t in_seq)
{
    ThingModelJsonObj_t *arr = ThingModelJson_GetItem(root, "data");
    int n = arr ? ThingModelJson_GetArraySize(arr) : 0;
    if (n <= 0) return;

    ThingModelJsonObj_t *reply_data = ThingModelJson_NewObject();
    if (!reply_data) return;

    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(im->session);
    for (int i = 0; i < n; i++) {
        ThingModelJsonObj_t *item = ThingModelJson_GetArrayItem(arr, i);
        const char *fname = item ? ThingModelJson_GetString(item, NULL) : NULL;
        if (!fname) continue;

        /* Copy out the current field's handler inside the lock; execute outside the lock. */
        PropHandlerEntry_t snap = {0};
        bool found = false;
        o->mutex_lock(im->lock);
        for (PropHandlerEntry_t *e = im->prop_handlers; e; e = e->next) {
            if (strcmp(e->field, fname) == 0) {
                snap = *e;
                found = (snap.get_handler != NULL);
                break;
            }
        }
        o->mutex_unlock(im->lock);
        if (!found) continue;   /* field with no registered getter: skipped in the reply */

        switch (snap.vtype) {
        case PVT_INT: case PVT_ENUM: {
            int64_t v = 0;
            T_ThingModelReturnCode rc = ((_PropGetIntH)snap.get_handler)(im, snap.get_user, &v);
            if (THING_MODEL_IS_OK(rc))
                ThingModelJson_AddInt(reply_data, fname, v);
            break;
        }
        case PVT_DOUBLE: {
            double v = 0;
            T_ThingModelReturnCode rc = ((_PropGetDblH)snap.get_handler)(im, snap.get_user, &v);
            if (THING_MODEL_IS_OK(rc))
                ThingModelJson_AddDouble(reply_data, fname, v);
            break;
        }
        case PVT_BOOL: {
            bool v = false;
            T_ThingModelReturnCode rc = ((_PropGetBoolH)snap.get_handler)(im, snap.get_user, &v);
            if (THING_MODEL_IS_OK(rc))
                ThingModelJson_AddBool(reply_data, fname, v);
            break;
        }
        case PVT_STRING: {
            char buf[256];
            buf[0] = 0;
            T_ThingModelReturnCode rc = ((_PropGetStrH)snap.get_handler)(im, snap.get_user, buf, sizeof(buf));
            if (THING_MODEL_IS_OK(rc))
                ThingModelJson_AddString(reply_data, fname, buf);
            break;
        }
        }
    }

    /* Wrap into {"seq":N,"data":{...}} and send it out. */
    ThingModelJsonObj_t *wire = ThingModelJson_NewObject();
    if (wire) {
        ThingModelJson_AddUInt  (wire, "seq",  (uint64_t)in_seq);
        ThingModelJson_AddObject(wire, "data", reply_data);
        char *s = ThingModelJson_PrintCompact(wire);
        if (s) {
            ThingModelClientSession_Publish(im->session, im->topic_prop_get_reply, s, 0);
            ThingModelMem_Free(s);
        }
        ThingModelJson_Free(wire);   /* free the whole tree; reply_data is already attached to wire */
    } else {
        ThingModelJson_Free(reply_data);
    }
}

/* ============================================================ */
/* PROPERTY SET dispatch                                         */
/* ============================================================ */
/* wire request: {"seq":N,"data":{"field_a":<value>, "field_b":<value>, ...}}
 * wire reply:   {"seq":N,"data":{"field_a":{"result":0,"result_message":"ok"}, ...}}
 *               field with no registered setter: result=THING_MODEL_RC_NOT_REGISTERED. */
static void add_set_field_result(ThingModelJsonObj_t *reply_data, const char *fname,
                                 T_ThingModelReturnCode rc, const char *msg)
{
    ThingModelJsonObj_t *o = ThingModelJson_NewObject();
    if (!o) return;
    ThingModelJson_AddInt   (o, "result",         (int64_t)rc);
    ThingModelJson_AddString(o, "result_message", msg ? msg : (THING_MODEL_IS_OK(rc) ? "ok" : "fail"));
    ThingModelJson_AddObject(reply_data, fname, o);
}

static void dispatch_prop_set(TimeSyncImpl_t *im, ThingModelJsonObj_t *root, uint32_t in_seq)
{
    ThingModelJsonObj_t *data = ThingModelJson_GetItem(root, "data");
    if (!data || !ThingModelJson_IsObject(data)) return;

    ThingModelJsonObj_t *reply_data = ThingModelJson_NewObject();
    if (!reply_data) return;

    /* We can't get the object's key list — so invert it: iterate the registered setters and check whether data has that key.
     * Upside: no need for the codec to expose an obj-iteration API;
     * Cost: if the cloud sends 100 unregistered fields → impl doesn't see them (and doesn't error either);
     *       in the normal case only a handful of fields are ever registered, so O(registration count) is the same order as iterating incoming keys. */
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(im->session);

    /* Snapshot the whole table's setter references inside the lock, execute outside the lock (to avoid self-deadlock from holding the lock). */
    typedef struct { char field[64]; EPropValueType vtype; void *h; void *u; } SetSnap_t;
    enum { STACK_CAP = 16 };
    SetSnap_t  stack_buf[STACK_CAP];
    SetSnap_t *snap_buf = stack_buf;
    int        snap_cap = STACK_CAP, snap_n = 0;

    o->mutex_lock(im->lock);
    for (PropHandlerEntry_t *e = im->prop_handlers; e; e = e->next) {
        if (!e->set_handler) continue;
        if (snap_n == snap_cap) {
            int nc = snap_cap * 2;
            SetSnap_t *nb;
            if (snap_buf == stack_buf) {
                nb = (SetSnap_t *)ThingModelMem_Alloc(nc * sizeof(SetSnap_t));
                if (nb) memcpy(nb, stack_buf, snap_n * sizeof(SetSnap_t));
            } else {
                /* OSAL has no realloc semantics, so alloc+copy+free manually */
                nb = (SetSnap_t *)ThingModelMem_Alloc(nc * sizeof(SetSnap_t));
                if (nb) {
                    memcpy(nb, snap_buf, snap_n * sizeof(SetSnap_t));
                    ThingModelMem_Free(snap_buf);
                }
            }
            if (!nb) break;
            snap_buf = nb; snap_cap = nc;
        }
        snprintf(snap_buf[snap_n].field, sizeof(snap_buf[snap_n].field), "%s", e->field);
        snap_buf[snap_n].vtype = e->vtype;
        snap_buf[snap_n].h     = e->set_handler;
        snap_buf[snap_n].u     = e->set_user;
        snap_n++;
    }
    o->mutex_unlock(im->lock);

    for (int i = 0; i < snap_n; i++) {
        ThingModelJsonObj_t *vobj = ThingModelJson_GetItem(data, snap_buf[i].field);
        if (!vobj) continue;     /* incoming didn't carry this field — not a set */
        T_ThingModelReturnCode rc = THING_MODEL_RC_OK;
        switch (snap_buf[i].vtype) {
        case PVT_INT: case PVT_ENUM:
            rc = ((_PropSetIntH)snap_buf[i].h)(im, snap_buf[i].u, ThingModelJson_GetInt(vobj, 0));
            break;
        case PVT_DOUBLE:
            rc = ((_PropSetDblH)snap_buf[i].h)(im, snap_buf[i].u, ThingModelJson_GetDouble(vobj, 0));
            break;
        case PVT_BOOL:
            rc = ((_PropSetBoolH)snap_buf[i].h)(im, snap_buf[i].u, ThingModelJson_GetBool(vobj, false));
            break;
        case PVT_STRING:
            rc = ((_PropSetStrH)snap_buf[i].h)(im, snap_buf[i].u, ThingModelJson_GetString(vobj, ""));
            break;
        }
        add_set_field_result(reply_data, snap_buf[i].field, rc, NULL);
    }
    if (snap_buf != stack_buf) ThingModelMem_Free(snap_buf);

    ThingModelJsonObj_t *wire = ThingModelJson_NewObject();
    if (wire) {
        ThingModelJson_AddUInt  (wire, "seq",  (uint64_t)in_seq);
        ThingModelJson_AddObject(wire, "data", reply_data);
        char *s = ThingModelJson_PrintCompact(wire);
        if (s) {
            ThingModelClientSession_Publish(im->session, im->topic_prop_set_reply, s, 0);
            ThingModelMem_Free(s);
        }
        ThingModelJson_Free(wire);
    } else {
        ThingModelJson_Free(reply_data);
    }
}

/* ============================================================ */
/* on_message main entry                                         */
/* ============================================================ */

static void on_session_message(void *user, const char *topic,
                               const char *payload, size_t len)
{
    TimeSyncImpl_t *im = user;

    int kind = -1;
    if      (strcmp(topic, im->topic_services) == 0) kind = 0;
    else if (strcmp(topic, im->topic_prop_get) == 0) kind = 1;
    else if (strcmp(topic, im->topic_prop_set) == 0) kind = 2;
    else return;

    ThingModelJsonObj_t *root = ThingModelJson_ParseN(payload, len);
    if (!root) return;
    uint32_t in_seq = (uint32_t)ThingModelJson_LookupInt(root, "seq", 0);

    if (kind == 0) {
        const char *in_method = ThingModelJson_LookupString(root, "method", NULL);
        if (in_method) dispatch_service(im, root, in_seq, in_method);
    } else if (kind == 1) {
        dispatch_prop_get(im, root, in_seq);
    } else {
        dispatch_prop_set(im, root, in_seq);
    }

    ThingModelJson_Free(root);
}

/* ============================================================ */
/* Create / Destroy                                              */
/* ============================================================ */

TimeSyncImpl_t *TimeSyncImpl_Create(ThingModelClientSession_t *session, const char *device_id)
{
    if (!session || !device_id || !device_id[0]) return NULL;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(session);
    TimeSyncImpl_t *im = ThingModelMem_Calloc(1, sizeof(*im));
    if (!im) return NULL;
    im->session = session;
    im->style   = ThingModelClientSession_GetTopicStyle(session);
    im->lock    = o->mutex_create();
    snprintf(im->device_id, sizeof(im->device_id), "%s", device_id);

    build_impl_topic(im->topic_services,       sizeof(im->topic_services),       im->style, im->device_id, THING_MODEL_TOPIC_SERVICE);
    build_impl_topic(im->topic_services_reply, sizeof(im->topic_services_reply), im->style, im->device_id, THING_MODEL_TOPIC_SERVICE_REPLY);
    build_impl_topic(im->topic_prop_get,       sizeof(im->topic_prop_get),       im->style, im->device_id, THING_MODEL_TOPIC_PROP_GET);
    build_impl_topic(im->topic_prop_get_reply, sizeof(im->topic_prop_get_reply), im->style, im->device_id, THING_MODEL_TOPIC_PROP_GET_REPLY);
    build_impl_topic(im->topic_prop_set,       sizeof(im->topic_prop_set),       im->style, im->device_id, THING_MODEL_TOPIC_PROP_SET);
    build_impl_topic(im->topic_prop_set_reply, sizeof(im->topic_prop_set_reply), im->style, im->device_id, THING_MODEL_TOPIC_PROP_SET_REPLY);

    im->listener_handle = ThingModelClientSession_AddListener(session, on_session_message, im);
    ThingModelClientSession_Subscribe(session, im->topic_services, 0);
    ThingModelClientSession_Subscribe(session, im->topic_prop_get, 0);
    ThingModelClientSession_Subscribe(session, im->topic_prop_set, 0);
    return im;
}

void TimeSyncImpl_Destroy(TimeSyncImpl_t *im)
{
    /* Do not call this function from inside a handler — the transport thread will self-deadlock on session.listener_lock. */
    if (!im) return;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(im->session);
    ThingModelClientSession_RemoveListener(im->session, im->listener_handle);
    ThingModelClientSession_Unsubscribe(im->session, im->topic_services);
    ThingModelClientSession_Unsubscribe(im->session, im->topic_prop_get);
    ThingModelClientSession_Unsubscribe(im->session, im->topic_prop_set);
    SvcHandlerEntry_t *se = im->svc_handlers;
    while (se) { SvcHandlerEntry_t *n = se->next; ThingModelMem_Free(se); se = n; }
    PropHandlerEntry_t *pe = im->prop_handlers;
    while (pe) { PropHandlerEntry_t *n = pe->next; ThingModelMem_Free(pe); pe = n; }
    if (im->lock) o->mutex_destroy(im->lock);
    ThingModelMem_Free(im);
}

/* ============================================================ */
/* Internal: SERVICE registration helper                         */
/* ============================================================ */

static T_ThingModelReturnCode register_svc_handler(TimeSyncImpl_t *im,
                                                   const char *method,
                                                   EHandlerKind kind,
                                                   void *handler, void *user,
                                                   _ReqDecoder req_decoder,
                                                   _ReplyEncoder reply_encoder,
                                                   _StructFree req_free,
                                                   _StructFree reply_free,
                                                   size_t req_size, size_t reply_size)
{
    if (!im || !method) return THING_MODEL_RC_INVALID_ARG;
    const T_ThingModelOsal *o = ThingModelClientSession_GetOsal(im->session);

    o->mutex_lock(im->lock);
    SvcHandlerEntry_t *e = im->svc_handlers;
    while (e) {
        if (strcmp(e->method, method) == 0) break;
        e = e->next;
    }
    if (handler == NULL) {
        if (e) {
            SvcHandlerEntry_t **pp = &im->svc_handlers;
            while (*pp != e) pp = &(*pp)->next;
            *pp = e->next;
            ThingModelMem_Free(e);
        }
        o->mutex_unlock(im->lock);
        return THING_MODEL_RC_OK;
    }
    if (!e) {
        e = ThingModelMem_Calloc(1, sizeof(*e));
        if (!e) { o->mutex_unlock(im->lock); return THING_MODEL_RC_OUT_OF_MEMORY; }
        snprintf(e->method, sizeof(e->method), "%s", method);
        e->next = im->svc_handlers;
        im->svc_handlers = e;
    }
    e->kind          = kind;
    e->handler       = handler;
    e->user          = user;
    e->req_decoder   = req_decoder;
    e->reply_encoder = reply_encoder;
    e->req_free      = req_free;
    e->reply_free    = reply_free;
    e->req_size      = req_size;
    e->reply_size    = reply_size;
    o->mutex_unlock(im->lock);
    return THING_MODEL_RC_OK;
}

/* ============================================================ */
/* SERVICE per-rpc registration API                              */
/* ============================================================ */

T_ThingModelReturnCode TimeSyncImpl_OnSetSignalMode(TimeSyncImpl_t *impl, TimeSyncImpl_SetSignalMode_Handler h, void *user)
{
    char method[128];
    if (impl) {
        if (impl->style == THING_MODEL_TOPIC_STYLE_V2)
            snprintf(method, sizeof(method), "%s_%s", TIME_SYNC_TOPIC_CAP_NAME, TIME_SYNC_METHOD_SET_SIGNAL_MODE);
        else
            snprintf(method, sizeof(method), "%s", TIME_SYNC_METHOD_SET_SIGNAL_MODE);
    } else {
        method[0] = 0;
    }
    return register_svc_handler(impl, method, HANDLER_COMMON,
                            (void *)h, user,
                            (_ReqDecoder)TimeSyncJson_DecodeTimeSyncSetSignalModeRequest,
                            NULL,
                            (_StructFree)TimeSync_FreeTimeSyncSetSignalModeRequest,
                            NULL,                              /* CommonReply has no heap fields */
                            sizeof(T_TimeSyncSetSignalModeRequest),
                            sizeof(T_ThingModelCommonReply));
}


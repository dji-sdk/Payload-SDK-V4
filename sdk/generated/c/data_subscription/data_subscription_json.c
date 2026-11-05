/**
 * @file    data_subscription_json.c
 */
#include "data_subscription_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode DataSubscriptionJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    ThingModelJsonObj_t *data = ThingModelJson_GetItem(r, "data");
    if (data) {
        out->result = (int32_t)ThingModelJson_LookupInt(data, "result", 0);
        snprintf(out->result_message, sizeof(out->result_message), "%s",
                 ThingModelJson_LookupString(data, "result_message", ""));
    }
    ThingModelJson_Free(r);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
{
    if (!json || !prop || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    ThingModelJsonObj_t *data = ThingModelJson_GetItem(r, "data");
    ThingModelJsonObj_t *pobj = data ? ThingModelJson_GetItem(data, prop) : NULL;
    if (pobj) {
        out->result = (int32_t)ThingModelJson_LookupInt(pobj, "result", 0);
        snprintf(out->result_message, sizeof(out->result_message), "%s",
                 ThingModelJson_LookupString(pobj, "result_message", ""));
    }
    ThingModelJson_Free(r);
    return THING_MODEL_RC_OK;
}

/* ============================================================ */
/* request / property encoding                                  */
/* ============================================================ */

char *DataSubscriptionJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    ThingModelJson_AddUInt(root, "seq", seq);
    ThingModelJson_AddString(root, "method", method);
    if (data_json && data_json[0] == '{') {
        ThingModelJsonObj_t *data = ThingModelJson_Parse(data_json);
        if (data) ThingModelJson_AddObject(root, "data", data);
        else      ThingModelJson_AddObject(root, "data", ThingModelJson_NewObject());
    } else {
        ThingModelJson_AddObject(root, "data", ThingModelJson_NewObject());
    }
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

char *DataSubscriptionJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    ThingModelJson_AddUInt(root, "seq", seq);
    ThingModelJsonObj_t *arr = ThingModelJson_NewArray();
    for (int i = 0; i < n_props; i++) ThingModelJson_ArrAppendString(arr, props[i]);
    ThingModelJson_AddArray(root, "data", arr);
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

char *DataSubscriptionJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    ThingModelJson_AddUInt(root, "seq", seq);
    ThingModelJsonObj_t *data = data_json ? ThingModelJson_Parse(data_json) : ThingModelJson_NewObject();
    if (!data) data = ThingModelJson_NewObject();
    ThingModelJson_AddObject(root, "data", data);
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

/* ============================================================ */
/* per-message encode / decode / free                          */
/* ============================================================ */

/* ============================================================
 * DataSubscriptionSubscribeTopicRequest
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionSubscribeTopicRequest_AsObj(const T_DataSubscriptionSubscribeTopicRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->topic) ThingModelJson_AddString(root, "topic", p->topic);
    ThingModelJson_AddInt(root, "frequency", (int64_t)p->frequency);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionSubscribeTopicRequest(const T_DataSubscriptionSubscribeTopicRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionSubscribeTopicRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionSubscribeTopicRequest_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionSubscribeTopicRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "topic", NULL);
        if (_s) out->topic = ThingModelMem_Strdup(_s);
    }
    out->frequency = (E_DataSubscriptionFrequency)ThingModelJson_LookupInt(src, "frequency", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionSubscribeTopicRequest(const char *json, T_DataSubscriptionSubscribeTopicRequest *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionSubscribeTopicRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionSubscribeTopicRequest(T_DataSubscriptionSubscribeTopicRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->topic); p->topic = NULL;
}

/* ============================================================
 * DataSubscriptionUnsubscribeTopicRequest
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionUnsubscribeTopicRequest_AsObj(const T_DataSubscriptionUnsubscribeTopicRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->topic) ThingModelJson_AddString(root, "topic", p->topic);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionUnsubscribeTopicRequest(const T_DataSubscriptionUnsubscribeTopicRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionUnsubscribeTopicRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionUnsubscribeTopicRequest_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionUnsubscribeTopicRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "topic", NULL);
        if (_s) out->topic = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionUnsubscribeTopicRequest(const char *json, T_DataSubscriptionUnsubscribeTopicRequest *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionUnsubscribeTopicRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionUnsubscribeTopicRequest(T_DataSubscriptionUnsubscribeTopicRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->topic); p->topic = NULL;
}

/* ============================================================
 * DataSubscriptionGetLatestValueRequest
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGetLatestValueRequest_AsObj(const T_DataSubscriptionGetLatestValueRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->topic) ThingModelJson_AddString(root, "topic", p->topic);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGetLatestValueRequest(const T_DataSubscriptionGetLatestValueRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGetLatestValueRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGetLatestValueRequest_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGetLatestValueRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "topic", NULL);
        if (_s) out->topic = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGetLatestValueRequest(const char *json, T_DataSubscriptionGetLatestValueRequest *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGetLatestValueRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGetLatestValueRequest(T_DataSubscriptionGetLatestValueRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->topic); p->topic = NULL;
}

/* ============================================================
 * DataSubscriptionQuaternion
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionQuaternion_AsObj(const T_DataSubscriptionQuaternion *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "q0", (double)p->q0);
    ThingModelJson_AddDouble(root, "q1", (double)p->q1);
    ThingModelJson_AddDouble(root, "q2", (double)p->q2);
    ThingModelJson_AddDouble(root, "q3", (double)p->q3);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionQuaternion(const T_DataSubscriptionQuaternion *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionQuaternion_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionQuaternion_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionQuaternion *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->q0 = (float)ThingModelJson_LookupDouble(src, "q0", 0);
    out->q1 = (float)ThingModelJson_LookupDouble(src, "q1", 0);
    out->q2 = (float)ThingModelJson_LookupDouble(src, "q2", 0);
    out->q3 = (float)ThingModelJson_LookupDouble(src, "q3", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionQuaternion(const char *json, T_DataSubscriptionQuaternion *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionQuaternion_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionQuaternion(T_DataSubscriptionQuaternion *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionVector3f
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(const T_DataSubscriptionVector3f *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "x", (double)p->x);
    ThingModelJson_AddDouble(root, "y", (double)p->y);
    ThingModelJson_AddDouble(root, "z", (double)p->z);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionVector3f(const T_DataSubscriptionVector3f *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionVector3f *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->x = (float)ThingModelJson_LookupDouble(src, "x", 0);
    out->y = (float)ThingModelJson_LookupDouble(src, "y", 0);
    out->z = (float)ThingModelJson_LookupDouble(src, "z", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3f(const char *json, T_DataSubscriptionVector3f *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionVector3f(T_DataSubscriptionVector3f *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionVelocity
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionVelocity_AsObj(const T_DataSubscriptionVelocity *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "x", (double)p->x);
    ThingModelJson_AddDouble(root, "y", (double)p->y);
    ThingModelJson_AddDouble(root, "z", (double)p->z);
    ThingModelJson_AddInt(root, "health", (int64_t)p->health);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionVelocity(const T_DataSubscriptionVelocity *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionVelocity_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVelocity_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionVelocity *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->x = (float)ThingModelJson_LookupDouble(src, "x", 0);
    out->y = (float)ThingModelJson_LookupDouble(src, "y", 0);
    out->z = (float)ThingModelJson_LookupDouble(src, "z", 0);
    out->health = (int32_t)ThingModelJson_LookupInt(src, "health", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVelocity(const char *json, T_DataSubscriptionVelocity *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionVelocity_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionVelocity(T_DataSubscriptionVelocity *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionAltitude
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(const T_DataSubscriptionAltitude *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "altitude", (double)p->altitude);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionAltitude(const T_DataSubscriptionAltitude *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionAltitude *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->altitude = (float)ThingModelJson_LookupDouble(src, "altitude", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAltitude(const char *json, T_DataSubscriptionAltitude *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionAltitude(T_DataSubscriptionAltitude *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionHeight
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionHeight_AsObj(const T_DataSubscriptionHeight *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "height", (double)p->height);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionHeight(const T_DataSubscriptionHeight *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionHeight_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHeight_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionHeight *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->height = (float)ThingModelJson_LookupDouble(src, "height", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHeight(const char *json, T_DataSubscriptionHeight *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionHeight_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionHeight(T_DataSubscriptionHeight *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionPositionFused
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionPositionFused_AsObj(const T_DataSubscriptionPositionFused *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "longitude", (double)p->longitude);
    ThingModelJson_AddDouble(root, "latitude", (double)p->latitude);
    ThingModelJson_AddDouble(root, "altitude", (double)p->altitude);
    ThingModelJson_AddInt(root, "visible_satellite_number", (int64_t)p->visible_satellite_number);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionPositionFused(const T_DataSubscriptionPositionFused *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionPositionFused_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionFused_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionPositionFused *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->longitude = (double)ThingModelJson_LookupDouble(src, "longitude", 0);
    out->latitude = (double)ThingModelJson_LookupDouble(src, "latitude", 0);
    out->altitude = (float)ThingModelJson_LookupDouble(src, "altitude", 0);
    out->visible_satellite_number = (int32_t)ThingModelJson_LookupInt(src, "visible_satellite_number", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionFused(const char *json, T_DataSubscriptionPositionFused *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionPositionFused_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionPositionFused(T_DataSubscriptionPositionFused *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionPositionVo
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionPositionVo_AsObj(const T_DataSubscriptionPositionVo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "x", (double)p->x);
    ThingModelJson_AddDouble(root, "y", (double)p->y);
    ThingModelJson_AddDouble(root, "z", (double)p->z);
    ThingModelJson_AddInt(root, "x_health", (int64_t)p->x_health);
    ThingModelJson_AddInt(root, "y_health", (int64_t)p->y_health);
    ThingModelJson_AddInt(root, "z_health", (int64_t)p->z_health);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionPositionVo(const T_DataSubscriptionPositionVo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionPositionVo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionVo_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionPositionVo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->x = (float)ThingModelJson_LookupDouble(src, "x", 0);
    out->y = (float)ThingModelJson_LookupDouble(src, "y", 0);
    out->z = (float)ThingModelJson_LookupDouble(src, "z", 0);
    out->x_health = (int32_t)ThingModelJson_LookupInt(src, "x_health", 0);
    out->y_health = (int32_t)ThingModelJson_LookupInt(src, "y_health", 0);
    out->z_health = (int32_t)ThingModelJson_LookupInt(src, "z_health", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionVo(const char *json, T_DataSubscriptionPositionVo *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionPositionVo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionPositionVo(T_DataSubscriptionPositionVo *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionGpsDate
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGpsDate_AsObj(const T_DataSubscriptionGpsDate *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "date", (int64_t)p->date);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGpsDate(const T_DataSubscriptionGpsDate *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGpsDate_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDate_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGpsDate *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->date = (int32_t)ThingModelJson_LookupInt(src, "date", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDate(const char *json, T_DataSubscriptionGpsDate *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGpsDate_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGpsDate(T_DataSubscriptionGpsDate *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionGpsTime
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGpsTime_AsObj(const T_DataSubscriptionGpsTime *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "time", (int64_t)p->time);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGpsTime(const T_DataSubscriptionGpsTime *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGpsTime_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsTime_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGpsTime *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->time = (int32_t)ThingModelJson_LookupInt(src, "time", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsTime(const char *json, T_DataSubscriptionGpsTime *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGpsTime_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGpsTime(T_DataSubscriptionGpsTime *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionVector3i
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionVector3i_AsObj(const T_DataSubscriptionVector3i *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "x", (int64_t)p->x);
    ThingModelJson_AddInt(root, "y", (int64_t)p->y);
    ThingModelJson_AddInt(root, "z", (int64_t)p->z);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionVector3i(const T_DataSubscriptionVector3i *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionVector3i_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3i_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionVector3i *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->x = (int32_t)ThingModelJson_LookupInt(src, "x", 0);
    out->y = (int32_t)ThingModelJson_LookupInt(src, "y", 0);
    out->z = (int32_t)ThingModelJson_LookupInt(src, "z", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3i(const char *json, T_DataSubscriptionVector3i *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionVector3i_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionVector3i(T_DataSubscriptionVector3i *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionGpsDetails
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGpsDetails_AsObj(const T_DataSubscriptionGpsDetails *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "hdop", (double)p->hdop);
    ThingModelJson_AddDouble(root, "pdop", (double)p->pdop);
    ThingModelJson_AddDouble(root, "fix_state", (double)p->fix_state);
    ThingModelJson_AddDouble(root, "vacc", (double)p->vacc);
    ThingModelJson_AddDouble(root, "hacc", (double)p->hacc);
    ThingModelJson_AddDouble(root, "sacc", (double)p->sacc);
    ThingModelJson_AddInt(root, "gps_satellite_number_used", (int64_t)p->gps_satellite_number_used);
    ThingModelJson_AddInt(root, "glonass_satellite_number_used", (int64_t)p->glonass_satellite_number_used);
    ThingModelJson_AddInt(root, "total_satellite_number_used", (int64_t)p->total_satellite_number_used);
    ThingModelJson_AddInt(root, "gps_counter", (int64_t)p->gps_counter);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGpsDetails(const T_DataSubscriptionGpsDetails *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGpsDetails_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDetails_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGpsDetails *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->hdop = (float)ThingModelJson_LookupDouble(src, "hdop", 0);
    out->pdop = (float)ThingModelJson_LookupDouble(src, "pdop", 0);
    out->fix_state = (float)ThingModelJson_LookupDouble(src, "fix_state", 0);
    out->vacc = (float)ThingModelJson_LookupDouble(src, "vacc", 0);
    out->hacc = (float)ThingModelJson_LookupDouble(src, "hacc", 0);
    out->sacc = (float)ThingModelJson_LookupDouble(src, "sacc", 0);
    out->gps_satellite_number_used = (int32_t)ThingModelJson_LookupInt(src, "gps_satellite_number_used", 0);
    out->glonass_satellite_number_used = (int32_t)ThingModelJson_LookupInt(src, "glonass_satellite_number_used", 0);
    out->total_satellite_number_used = (int32_t)ThingModelJson_LookupInt(src, "total_satellite_number_used", 0);
    out->gps_counter = (int32_t)ThingModelJson_LookupInt(src, "gps_counter", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDetails(const char *json, T_DataSubscriptionGpsDetails *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGpsDetails_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGpsDetails(T_DataSubscriptionGpsDetails *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionGpsSignalLevel
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel_AsObj(const T_DataSubscriptionGpsSignalLevel *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "gps_signal_level", (int64_t)p->gps_signal_level);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel(const T_DataSubscriptionGpsSignalLevel *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGpsSignalLevel *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->gps_signal_level = (int32_t)ThingModelJson_LookupInt(src, "gps_signal_level", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel(const char *json, T_DataSubscriptionGpsSignalLevel *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGpsSignalLevel(T_DataSubscriptionGpsSignalLevel *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRtkYaw
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRtkYaw_AsObj(const T_DataSubscriptionRtkYaw *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "yaw", (int64_t)p->yaw);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRtkYaw(const T_DataSubscriptionRtkYaw *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRtkYaw_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYaw_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRtkYaw *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->yaw = (int32_t)ThingModelJson_LookupInt(src, "yaw", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYaw(const char *json, T_DataSubscriptionRtkYaw *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRtkYaw_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRtkYaw(T_DataSubscriptionRtkYaw *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRtkPositionInfo
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo_AsObj(const T_DataSubscriptionRtkPositionInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "position_solution_properaty", (int64_t)p->position_solution_properaty);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo(const T_DataSubscriptionRtkPositionInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRtkPositionInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->position_solution_properaty = (int32_t)ThingModelJson_LookupInt(src, "position_solution_properaty", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo(const char *json, T_DataSubscriptionRtkPositionInfo *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRtkPositionInfo(T_DataSubscriptionRtkPositionInfo *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRtkYawInfo
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo_AsObj(const T_DataSubscriptionRtkYawInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "yaw_info", (int64_t)p->yaw_info);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo(const T_DataSubscriptionRtkYawInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRtkYawInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->yaw_info = (int32_t)ThingModelJson_LookupInt(src, "yaw_info", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo(const char *json, T_DataSubscriptionRtkYawInfo *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRtkYawInfo(T_DataSubscriptionRtkYawInfo *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRtkConnectStatus
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus_AsObj(const T_DataSubscriptionRtkConnectStatus *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "connected", (int64_t)p->connected);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus(const T_DataSubscriptionRtkConnectStatus *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRtkConnectStatus *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->connected = (int32_t)ThingModelJson_LookupInt(src, "connected", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus(const char *json, T_DataSubscriptionRtkConnectStatus *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRtkConnectStatus(T_DataSubscriptionRtkConnectStatus *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRc
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRc_AsObj(const T_DataSubscriptionRc *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "roll", (int64_t)p->roll);
    ThingModelJson_AddInt(root, "pitch", (int64_t)p->pitch);
    ThingModelJson_AddInt(root, "yaw", (int64_t)p->yaw);
    ThingModelJson_AddInt(root, "throttle", (int64_t)p->throttle);
    ThingModelJson_AddInt(root, "mode", (int64_t)p->mode);
    ThingModelJson_AddInt(root, "gear", (int64_t)p->gear);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRc(const T_DataSubscriptionRc *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRc_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRc_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRc *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->roll = (int32_t)ThingModelJson_LookupInt(src, "roll", 0);
    out->pitch = (int32_t)ThingModelJson_LookupInt(src, "pitch", 0);
    out->yaw = (int32_t)ThingModelJson_LookupInt(src, "yaw", 0);
    out->throttle = (int32_t)ThingModelJson_LookupInt(src, "throttle", 0);
    out->mode = (int32_t)ThingModelJson_LookupInt(src, "mode", 0);
    out->gear = (int32_t)ThingModelJson_LookupInt(src, "gear", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRc(const char *json, T_DataSubscriptionRc *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRc_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRc(T_DataSubscriptionRc *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionRcWithFlagData
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData_AsObj(const T_DataSubscriptionRcWithFlagData *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "pitch", (double)p->pitch);
    ThingModelJson_AddDouble(root, "roll", (double)p->roll);
    ThingModelJson_AddDouble(root, "yaw", (double)p->yaw);
    ThingModelJson_AddDouble(root, "throttle", (double)p->throttle);
    ThingModelJson_AddInt(root, "logic_connected", (int64_t)p->logic_connected);
    ThingModelJson_AddInt(root, "sky_connected", (int64_t)p->sky_connected);
    ThingModelJson_AddInt(root, "app_connected", (int64_t)p->app_connected);
    ThingModelJson_AddInt(root, "ground_connected", (int64_t)p->ground_connected);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData(const T_DataSubscriptionRcWithFlagData *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionRcWithFlagData *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->pitch = (float)ThingModelJson_LookupDouble(src, "pitch", 0);
    out->roll = (float)ThingModelJson_LookupDouble(src, "roll", 0);
    out->yaw = (float)ThingModelJson_LookupDouble(src, "yaw", 0);
    out->throttle = (float)ThingModelJson_LookupDouble(src, "throttle", 0);
    out->logic_connected = (int32_t)ThingModelJson_LookupInt(src, "logic_connected", 0);
    out->sky_connected = (int32_t)ThingModelJson_LookupInt(src, "sky_connected", 0);
    out->app_connected = (int32_t)ThingModelJson_LookupInt(src, "app_connected", 0);
    out->ground_connected = (int32_t)ThingModelJson_LookupInt(src, "ground_connected", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData(const char *json, T_DataSubscriptionRcWithFlagData *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionRcWithFlagData(T_DataSubscriptionRcWithFlagData *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionStatusFlight
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionStatusFlight_AsObj(const T_DataSubscriptionStatusFlight *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "status", (int64_t)p->status);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionStatusFlight(const T_DataSubscriptionStatusFlight *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionStatusFlight_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusFlight_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionStatusFlight *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->status = (int32_t)ThingModelJson_LookupInt(src, "status", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusFlight(const char *json, T_DataSubscriptionStatusFlight *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionStatusFlight_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionStatusFlight(T_DataSubscriptionStatusFlight *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionStatusDisplayMode
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode_AsObj(const T_DataSubscriptionStatusDisplayMode *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "status", (int64_t)p->status);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode(const T_DataSubscriptionStatusDisplayMode *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionStatusDisplayMode *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->status = (int32_t)ThingModelJson_LookupInt(src, "status", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode(const char *json, T_DataSubscriptionStatusDisplayMode *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionStatusDisplayMode(T_DataSubscriptionStatusDisplayMode *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionControlDevice
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionControlDevice_AsObj(const T_DataSubscriptionControlDevice *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "control_authority", (int64_t)p->control_authority);
    ThingModelJson_AddInt(root, "control_authority_change_reason", (int64_t)p->control_authority_change_reason);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionControlDevice(const T_DataSubscriptionControlDevice *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionControlDevice_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionControlDevice_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionControlDevice *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->control_authority = (int32_t)ThingModelJson_LookupInt(src, "control_authority", 0);
    out->control_authority_change_reason = (int32_t)ThingModelJson_LookupInt(src, "control_authority_change_reason", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionControlDevice(const char *json, T_DataSubscriptionControlDevice *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionControlDevice_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionControlDevice(T_DataSubscriptionControlDevice *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionEscItem
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionEscItem_AsObj(const T_DataSubscriptionEscItem *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "current", (int64_t)p->current);
    ThingModelJson_AddInt(root, "speed", (int64_t)p->speed);
    ThingModelJson_AddInt(root, "voltage", (int64_t)p->voltage);
    ThingModelJson_AddInt(root, "temperature", (int64_t)p->temperature);
    ThingModelJson_AddInt(root, "stall", (int64_t)p->stall);
    ThingModelJson_AddInt(root, "empty", (int64_t)p->empty);
    ThingModelJson_AddInt(root, "unbalanced", (int64_t)p->unbalanced);
    ThingModelJson_AddInt(root, "esc_disconnected", (int64_t)p->esc_disconnected);
    ThingModelJson_AddInt(root, "temperature_high", (int64_t)p->temperature_high);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionEscItem(const T_DataSubscriptionEscItem *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionEscItem_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionEscItem_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionEscItem *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->current = (int32_t)ThingModelJson_LookupInt(src, "current", 0);
    out->speed = (int32_t)ThingModelJson_LookupInt(src, "speed", 0);
    out->voltage = (int32_t)ThingModelJson_LookupInt(src, "voltage", 0);
    out->temperature = (int32_t)ThingModelJson_LookupInt(src, "temperature", 0);
    out->stall = (int32_t)ThingModelJson_LookupInt(src, "stall", 0);
    out->empty = (int32_t)ThingModelJson_LookupInt(src, "empty", 0);
    out->unbalanced = (int32_t)ThingModelJson_LookupInt(src, "unbalanced", 0);
    out->esc_disconnected = (int32_t)ThingModelJson_LookupInt(src, "esc_disconnected", 0);
    out->temperature_high = (int32_t)ThingModelJson_LookupInt(src, "temperature_high", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionEscItem(const char *json, T_DataSubscriptionEscItem *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionEscItem_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionEscItem(T_DataSubscriptionEscItem *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionFlightAnomaly
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly_AsObj(const T_DataSubscriptionFlightAnomaly *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "impact_in_air", (int64_t)p->impact_in_air);
    ThingModelJson_AddInt(root, "random_fly", (int64_t)p->random_fly);
    ThingModelJson_AddInt(root, "height_ctrl_fail", (int64_t)p->height_ctrl_fail);
    ThingModelJson_AddInt(root, "roll_pitch_ctrl_fail", (int64_t)p->roll_pitch_ctrl_fail);
    ThingModelJson_AddInt(root, "yaw_ctrl_fail", (int64_t)p->yaw_ctrl_fail);
    ThingModelJson_AddInt(root, "aircraft_is_falling", (int64_t)p->aircraft_is_falling);
    ThingModelJson_AddInt(root, "strong_wind_level1", (int64_t)p->strong_wind_level1);
    ThingModelJson_AddInt(root, "strong_wind_level2", (int64_t)p->strong_wind_level2);
    ThingModelJson_AddInt(root, "compass_installation_error", (int64_t)p->compass_installation_error);
    ThingModelJson_AddInt(root, "imu_installation_error", (int64_t)p->imu_installation_error);
    ThingModelJson_AddInt(root, "esc_temperature_high", (int64_t)p->esc_temperature_high);
    ThingModelJson_AddInt(root, "at_least_one_esc_disconnected", (int64_t)p->at_least_one_esc_disconnected);
    ThingModelJson_AddInt(root, "gps_yaw_error", (int64_t)p->gps_yaw_error);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly(const T_DataSubscriptionFlightAnomaly *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionFlightAnomaly *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->impact_in_air = (int32_t)ThingModelJson_LookupInt(src, "impact_in_air", 0);
    out->random_fly = (int32_t)ThingModelJson_LookupInt(src, "random_fly", 0);
    out->height_ctrl_fail = (int32_t)ThingModelJson_LookupInt(src, "height_ctrl_fail", 0);
    out->roll_pitch_ctrl_fail = (int32_t)ThingModelJson_LookupInt(src, "roll_pitch_ctrl_fail", 0);
    out->yaw_ctrl_fail = (int32_t)ThingModelJson_LookupInt(src, "yaw_ctrl_fail", 0);
    out->aircraft_is_falling = (int32_t)ThingModelJson_LookupInt(src, "aircraft_is_falling", 0);
    out->strong_wind_level1 = (int32_t)ThingModelJson_LookupInt(src, "strong_wind_level1", 0);
    out->strong_wind_level2 = (int32_t)ThingModelJson_LookupInt(src, "strong_wind_level2", 0);
    out->compass_installation_error = (int32_t)ThingModelJson_LookupInt(src, "compass_installation_error", 0);
    out->imu_installation_error = (int32_t)ThingModelJson_LookupInt(src, "imu_installation_error", 0);
    out->esc_temperature_high = (int32_t)ThingModelJson_LookupInt(src, "esc_temperature_high", 0);
    out->at_least_one_esc_disconnected = (int32_t)ThingModelJson_LookupInt(src, "at_least_one_esc_disconnected", 0);
    out->gps_yaw_error = (int32_t)ThingModelJson_LookupInt(src, "gps_yaw_error", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly(const char *json, T_DataSubscriptionFlightAnomaly *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionFlightAnomaly(T_DataSubscriptionFlightAnomaly *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionAvoidData
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionAvoidData_AsObj(const T_DataSubscriptionAvoidData *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "down", (double)p->down);
    ThingModelJson_AddDouble(root, "front", (double)p->front);
    ThingModelJson_AddDouble(root, "right", (double)p->right);
    ThingModelJson_AddDouble(root, "back", (double)p->back);
    ThingModelJson_AddDouble(root, "left", (double)p->left);
    ThingModelJson_AddDouble(root, "up", (double)p->up);
    ThingModelJson_AddInt(root, "down_health", (int64_t)p->down_health);
    ThingModelJson_AddInt(root, "front_health", (int64_t)p->front_health);
    ThingModelJson_AddInt(root, "right_health", (int64_t)p->right_health);
    ThingModelJson_AddInt(root, "back_health", (int64_t)p->back_health);
    ThingModelJson_AddInt(root, "left_health", (int64_t)p->left_health);
    ThingModelJson_AddInt(root, "up_health", (int64_t)p->up_health);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionAvoidData(const T_DataSubscriptionAvoidData *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionAvoidData_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAvoidData_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionAvoidData *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->down = (float)ThingModelJson_LookupDouble(src, "down", 0);
    out->front = (float)ThingModelJson_LookupDouble(src, "front", 0);
    out->right = (float)ThingModelJson_LookupDouble(src, "right", 0);
    out->back = (float)ThingModelJson_LookupDouble(src, "back", 0);
    out->left = (float)ThingModelJson_LookupDouble(src, "left", 0);
    out->up = (float)ThingModelJson_LookupDouble(src, "up", 0);
    out->down_health = (int32_t)ThingModelJson_LookupInt(src, "down_health", 0);
    out->front_health = (int32_t)ThingModelJson_LookupInt(src, "front_health", 0);
    out->right_health = (int32_t)ThingModelJson_LookupInt(src, "right_health", 0);
    out->back_health = (int32_t)ThingModelJson_LookupInt(src, "back_health", 0);
    out->left_health = (int32_t)ThingModelJson_LookupInt(src, "left_health", 0);
    out->up_health = (int32_t)ThingModelJson_LookupInt(src, "up_health", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAvoidData(const char *json, T_DataSubscriptionAvoidData *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionAvoidData_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionAvoidData(T_DataSubscriptionAvoidData *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionHomePointSetStatus
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus_AsObj(const T_DataSubscriptionHomePointSetStatus *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "status", (int64_t)p->status);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus(const T_DataSubscriptionHomePointSetStatus *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionHomePointSetStatus *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->status = (int32_t)ThingModelJson_LookupInt(src, "status", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus(const char *json, T_DataSubscriptionHomePointSetStatus *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionHomePointSetStatus(T_DataSubscriptionHomePointSetStatus *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionHomePointInfo
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo_AsObj(const T_DataSubscriptionHomePointInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "longitude", (double)p->longitude);
    ThingModelJson_AddDouble(root, "latitude", (double)p->latitude);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo(const T_DataSubscriptionHomePointInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionHomePointInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->longitude = (double)ThingModelJson_LookupDouble(src, "longitude", 0);
    out->latitude = (double)ThingModelJson_LookupDouble(src, "latitude", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo(const char *json, T_DataSubscriptionHomePointInfo *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionHomePointInfo(T_DataSubscriptionHomePointInfo *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionBatteryState
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionBatteryState_AsObj(const T_DataSubscriptionBatteryState *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "cell_break", (int64_t)p->cell_break);
    ThingModelJson_AddInt(root, "self_check_error", (int64_t)p->self_check_error);
    ThingModelJson_AddInt(root, "battery_closed_reason", (int64_t)p->battery_closed_reason);
    ThingModelJson_AddInt(root, "bat_soh_state", (int64_t)p->bat_soh_state);
    ThingModelJson_AddInt(root, "max_cycle_limit", (int64_t)p->max_cycle_limit);
    ThingModelJson_AddInt(root, "less_battery", (int64_t)p->less_battery);
    ThingModelJson_AddInt(root, "battery_communication_abnormal", (int64_t)p->battery_communication_abnormal);
    ThingModelJson_AddInt(root, "has_cell_break", (int64_t)p->has_cell_break);
    ThingModelJson_AddInt(root, "is_battery_embed", (int64_t)p->is_battery_embed);
    ThingModelJson_AddInt(root, "heat_state", (int64_t)p->heat_state);
    ThingModelJson_AddInt(root, "soc_state", (int64_t)p->soc_state);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionBatteryState(const T_DataSubscriptionBatteryState *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionBatteryState_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatteryState_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionBatteryState *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->cell_break = (int32_t)ThingModelJson_LookupInt(src, "cell_break", 0);
    out->self_check_error = (int32_t)ThingModelJson_LookupInt(src, "self_check_error", 0);
    out->battery_closed_reason = (int32_t)ThingModelJson_LookupInt(src, "battery_closed_reason", 0);
    out->bat_soh_state = (int32_t)ThingModelJson_LookupInt(src, "bat_soh_state", 0);
    out->max_cycle_limit = (int32_t)ThingModelJson_LookupInt(src, "max_cycle_limit", 0);
    out->less_battery = (int32_t)ThingModelJson_LookupInt(src, "less_battery", 0);
    out->battery_communication_abnormal = (int32_t)ThingModelJson_LookupInt(src, "battery_communication_abnormal", 0);
    out->has_cell_break = (int32_t)ThingModelJson_LookupInt(src, "has_cell_break", 0);
    out->is_battery_embed = (int32_t)ThingModelJson_LookupInt(src, "is_battery_embed", 0);
    out->heat_state = (int32_t)ThingModelJson_LookupInt(src, "heat_state", 0);
    out->soc_state = (int32_t)ThingModelJson_LookupInt(src, "soc_state", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatteryState(const char *json, T_DataSubscriptionBatteryState *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionBatteryState_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionBatteryState(T_DataSubscriptionBatteryState *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionBatterySingleInfo
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo_AsObj(const T_DataSubscriptionBatterySingleInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "battery_index", (int64_t)p->battery_index);
    ThingModelJson_AddInt(root, "current_voltage", (int64_t)p->current_voltage);
    ThingModelJson_AddInt(root, "current_electric", (int64_t)p->current_electric);
    ThingModelJson_AddInt(root, "full_capacity", (int64_t)p->full_capacity);
    ThingModelJson_AddInt(root, "remained_capacity", (int64_t)p->remained_capacity);
    ThingModelJson_AddInt(root, "battery_temperature", (int64_t)p->battery_temperature);
    ThingModelJson_AddInt(root, "cell_count", (int64_t)p->cell_count);
    ThingModelJson_AddInt(root, "battery_capacity_percent", (int64_t)p->battery_capacity_percent);
    ThingModelJson_AddObject(root, "battery_state", DataSubscriptionJson_EncodeDataSubscriptionBatteryState_AsObj(&p->battery_state));
    ThingModelJson_AddInt(root, "SOP", (int64_t)p->SOP);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo(const T_DataSubscriptionBatterySingleInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionBatterySingleInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->battery_index = (int32_t)ThingModelJson_LookupInt(src, "battery_index", 0);
    out->current_voltage = (int32_t)ThingModelJson_LookupInt(src, "current_voltage", 0);
    out->current_electric = (int32_t)ThingModelJson_LookupInt(src, "current_electric", 0);
    out->full_capacity = (int32_t)ThingModelJson_LookupInt(src, "full_capacity", 0);
    out->remained_capacity = (int32_t)ThingModelJson_LookupInt(src, "remained_capacity", 0);
    out->battery_temperature = (int32_t)ThingModelJson_LookupInt(src, "battery_temperature", 0);
    out->cell_count = (int32_t)ThingModelJson_LookupInt(src, "cell_count", 0);
    out->battery_capacity_percent = (int32_t)ThingModelJson_LookupInt(src, "battery_capacity_percent", 0);
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "battery_state");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionBatteryState_FromObj(_o, &out->battery_state);
    }
    out->SOP = (int32_t)ThingModelJson_LookupInt(src, "SOP", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo(const char *json, T_DataSubscriptionBatterySingleInfo *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionBatterySingleInfo(T_DataSubscriptionBatterySingleInfo *p)
{
    if (!p) return;
    DataSubscription_FreeDataSubscriptionBatteryState(&p->battery_state);
}

/* ============================================================
 * DataSubscriptionGimbalAngles
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles_AsObj(const T_DataSubscriptionGimbalAngles *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddDouble(root, "x", (double)p->x);
    ThingModelJson_AddDouble(root, "y", (double)p->y);
    ThingModelJson_AddDouble(root, "z", (double)p->z);
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles(const T_DataSubscriptionGimbalAngles *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionGimbalAngles *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->x = (float)ThingModelJson_LookupDouble(src, "x", 0);
    out->y = (float)ThingModelJson_LookupDouble(src, "y", 0);
    out->z = (float)ThingModelJson_LookupDouble(src, "z", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles(const char *json, T_DataSubscriptionGimbalAngles *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionGimbalAngles(T_DataSubscriptionGimbalAngles *p)
{
    if (!p) return;
}

/* ============================================================
 * DataSubscriptionPublishDataPush
 * ============================================================ */

ThingModelJsonObj_t *DataSubscriptionJson_EncodeDataSubscriptionPublishDataPush_AsObj(const T_DataSubscriptionPublishDataPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddObject(root, "quaternion", DataSubscriptionJson_EncodeDataSubscriptionQuaternion_AsObj(&p->quaternion));
    ThingModelJson_AddObject(root, "acceleration_ground", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->acceleration_ground));
    ThingModelJson_AddObject(root, "acceleration_body", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->acceleration_body));
    ThingModelJson_AddObject(root, "acceleration_raw", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->acceleration_raw));
    ThingModelJson_AddObject(root, "velocity", DataSubscriptionJson_EncodeDataSubscriptionVelocity_AsObj(&p->velocity));
    ThingModelJson_AddObject(root, "angular_rate_fused", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->angular_rate_fused));
    ThingModelJson_AddObject(root, "angular_rate_raw", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->angular_rate_raw));
    ThingModelJson_AddObject(root, "altitude_fused", DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(&p->altitude_fused));
    ThingModelJson_AddObject(root, "altitude_barometer", DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(&p->altitude_barometer));
    ThingModelJson_AddObject(root, "altitude_of_homepoint", DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(&p->altitude_of_homepoint));
    ThingModelJson_AddObject(root, "height_fusion", DataSubscriptionJson_EncodeDataSubscriptionHeight_AsObj(&p->height_fusion));
    ThingModelJson_AddObject(root, "position_fused", DataSubscriptionJson_EncodeDataSubscriptionPositionFused_AsObj(&p->position_fused));
    ThingModelJson_AddObject(root, "position_vo", DataSubscriptionJson_EncodeDataSubscriptionPositionVo_AsObj(&p->position_vo));
    ThingModelJson_AddObject(root, "gps_date", DataSubscriptionJson_EncodeDataSubscriptionGpsDate_AsObj(&p->gps_date));
    ThingModelJson_AddObject(root, "gps_time", DataSubscriptionJson_EncodeDataSubscriptionGpsTime_AsObj(&p->gps_time));
    ThingModelJson_AddObject(root, "gps_position", DataSubscriptionJson_EncodeDataSubscriptionVector3i_AsObj(&p->gps_position));
    ThingModelJson_AddObject(root, "gps_velocity", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->gps_velocity));
    ThingModelJson_AddObject(root, "gps_details", DataSubscriptionJson_EncodeDataSubscriptionGpsDetails_AsObj(&p->gps_details));
    ThingModelJson_AddObject(root, "gps_signal_level", DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel_AsObj(&p->gps_signal_level));
    ThingModelJson_AddObject(root, "rtk_position", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->rtk_position));
    ThingModelJson_AddObject(root, "rtk_velocity", DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(&p->rtk_velocity));
    ThingModelJson_AddObject(root, "rtk_yaw", DataSubscriptionJson_EncodeDataSubscriptionRtkYaw_AsObj(&p->rtk_yaw));
    ThingModelJson_AddObject(root, "rtk_position_info", DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo_AsObj(&p->rtk_position_info));
    ThingModelJson_AddObject(root, "rtk_yaw_info", DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo_AsObj(&p->rtk_yaw_info));
    ThingModelJson_AddObject(root, "rtk_connect_status", DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus_AsObj(&p->rtk_connect_status));
    ThingModelJson_AddObject(root, "rc", DataSubscriptionJson_EncodeDataSubscriptionRc_AsObj(&p->rc));
    ThingModelJson_AddObject(root, "rc_with_flag_data", DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData_AsObj(&p->rc_with_flag_data));
    ThingModelJson_AddObject(root, "status_flight", DataSubscriptionJson_EncodeDataSubscriptionStatusFlight_AsObj(&p->status_flight));
    ThingModelJson_AddObject(root, "status_displaymode", DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode_AsObj(&p->status_displaymode));
    ThingModelJson_AddObject(root, "control_device", DataSubscriptionJson_EncodeDataSubscriptionControlDevice_AsObj(&p->control_device));
    ThingModelJson_AddObject(root, "compass", DataSubscriptionJson_EncodeDataSubscriptionVector3i_AsObj(&p->compass));
    ThingModelJson_AddObject(root, "flight_anomaly", DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly_AsObj(&p->flight_anomaly));
    ThingModelJson_AddObject(root, "avoid_data", DataSubscriptionJson_EncodeDataSubscriptionAvoidData_AsObj(&p->avoid_data));
    ThingModelJson_AddObject(root, "home_point_set_status", DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus_AsObj(&p->home_point_set_status));
    ThingModelJson_AddObject(root, "home_point_info", DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo_AsObj(&p->home_point_info));
    ThingModelJson_AddObject(root, "battery_single_info_index1", DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo_AsObj(&p->battery_single_info_index1));
    ThingModelJson_AddObject(root, "battery_single_info_index2", DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo_AsObj(&p->battery_single_info_index2));
    ThingModelJson_AddObject(root, "gimbal_angles", DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles_AsObj(&p->gimbal_angles));
    return root;
}

char *DataSubscriptionJson_EncodeDataSubscriptionPublishDataPush(const T_DataSubscriptionPublishDataPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DataSubscriptionJson_EncodeDataSubscriptionPublishDataPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPublishDataPush_FromObj(const ThingModelJsonObj_t *src, T_DataSubscriptionPublishDataPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "quaternion");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionQuaternion_FromObj(_o, &out->quaternion);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "acceleration_ground");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->acceleration_ground);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "acceleration_body");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->acceleration_body);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "acceleration_raw");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->acceleration_raw);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "velocity");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVelocity_FromObj(_o, &out->velocity);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "angular_rate_fused");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->angular_rate_fused);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "angular_rate_raw");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->angular_rate_raw);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "altitude_fused");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(_o, &out->altitude_fused);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "altitude_barometer");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(_o, &out->altitude_barometer);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "altitude_of_homepoint");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(_o, &out->altitude_of_homepoint);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "height_fusion");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionHeight_FromObj(_o, &out->height_fusion);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "position_fused");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionPositionFused_FromObj(_o, &out->position_fused);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "position_vo");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionPositionVo_FromObj(_o, &out->position_vo);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_date");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionGpsDate_FromObj(_o, &out->gps_date);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_time");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionGpsTime_FromObj(_o, &out->gps_time);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_position");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3i_FromObj(_o, &out->gps_position);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_velocity");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->gps_velocity);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_details");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionGpsDetails_FromObj(_o, &out->gps_details);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gps_signal_level");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel_FromObj(_o, &out->gps_signal_level);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_position");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->rtk_position);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_velocity");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(_o, &out->rtk_velocity);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_yaw");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRtkYaw_FromObj(_o, &out->rtk_yaw);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_position_info");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo_FromObj(_o, &out->rtk_position_info);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_yaw_info");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo_FromObj(_o, &out->rtk_yaw_info);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rtk_connect_status");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus_FromObj(_o, &out->rtk_connect_status);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rc");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRc_FromObj(_o, &out->rc);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "rc_with_flag_data");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData_FromObj(_o, &out->rc_with_flag_data);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "status_flight");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionStatusFlight_FromObj(_o, &out->status_flight);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "status_displaymode");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode_FromObj(_o, &out->status_displaymode);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "control_device");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionControlDevice_FromObj(_o, &out->control_device);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "compass");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionVector3i_FromObj(_o, &out->compass);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "flight_anomaly");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly_FromObj(_o, &out->flight_anomaly);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "avoid_data");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionAvoidData_FromObj(_o, &out->avoid_data);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "home_point_set_status");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus_FromObj(_o, &out->home_point_set_status);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "home_point_info");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo_FromObj(_o, &out->home_point_info);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "battery_single_info_index1");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo_FromObj(_o, &out->battery_single_info_index1);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "battery_single_info_index2");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo_FromObj(_o, &out->battery_single_info_index2);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "gimbal_angles");
        if (_o) DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles_FromObj(_o, &out->gimbal_angles);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPublishDataPush(const char *json, T_DataSubscriptionPublishDataPush *out)
{
    if (!json || !out) return THING_MODEL_RC_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    ThingModelJsonObj_t *r = ThingModelJson_Parse(json);
    if (!r) return THING_MODEL_RC_DECODE_ERROR;
    /* Accepts two input shapes: an outer {"data":{...}} wrapper / directly passing the data obj.
       Only drill down when the top-level "data" is an object (i.e. a genuine outer wrapper); otherwise it may be a field
       named "data" that the message itself carries (e.g. a bytes audio hex string), and drilling down by mistake would drop the whole payload. */
    const ThingModelJsonObj_t *root = r;
    {
        const ThingModelJsonObj_t *_wrap = ThingModelJson_GetItem(r, "data");
        if (_wrap && ThingModelJson_IsObject(_wrap)) root = _wrap;
    }
    T_ThingModelReturnCode rc = DataSubscriptionJson_DecodeDataSubscriptionPublishDataPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DataSubscription_FreeDataSubscriptionPublishDataPush(T_DataSubscriptionPublishDataPush *p)
{
    if (!p) return;
    DataSubscription_FreeDataSubscriptionQuaternion(&p->quaternion);
    DataSubscription_FreeDataSubscriptionVector3f(&p->acceleration_ground);
    DataSubscription_FreeDataSubscriptionVector3f(&p->acceleration_body);
    DataSubscription_FreeDataSubscriptionVector3f(&p->acceleration_raw);
    DataSubscription_FreeDataSubscriptionVelocity(&p->velocity);
    DataSubscription_FreeDataSubscriptionVector3f(&p->angular_rate_fused);
    DataSubscription_FreeDataSubscriptionVector3f(&p->angular_rate_raw);
    DataSubscription_FreeDataSubscriptionAltitude(&p->altitude_fused);
    DataSubscription_FreeDataSubscriptionAltitude(&p->altitude_barometer);
    DataSubscription_FreeDataSubscriptionAltitude(&p->altitude_of_homepoint);
    DataSubscription_FreeDataSubscriptionHeight(&p->height_fusion);
    DataSubscription_FreeDataSubscriptionPositionFused(&p->position_fused);
    DataSubscription_FreeDataSubscriptionPositionVo(&p->position_vo);
    DataSubscription_FreeDataSubscriptionGpsDate(&p->gps_date);
    DataSubscription_FreeDataSubscriptionGpsTime(&p->gps_time);
    DataSubscription_FreeDataSubscriptionVector3i(&p->gps_position);
    DataSubscription_FreeDataSubscriptionVector3f(&p->gps_velocity);
    DataSubscription_FreeDataSubscriptionGpsDetails(&p->gps_details);
    DataSubscription_FreeDataSubscriptionGpsSignalLevel(&p->gps_signal_level);
    DataSubscription_FreeDataSubscriptionVector3f(&p->rtk_position);
    DataSubscription_FreeDataSubscriptionVector3f(&p->rtk_velocity);
    DataSubscription_FreeDataSubscriptionRtkYaw(&p->rtk_yaw);
    DataSubscription_FreeDataSubscriptionRtkPositionInfo(&p->rtk_position_info);
    DataSubscription_FreeDataSubscriptionRtkYawInfo(&p->rtk_yaw_info);
    DataSubscription_FreeDataSubscriptionRtkConnectStatus(&p->rtk_connect_status);
    DataSubscription_FreeDataSubscriptionRc(&p->rc);
    DataSubscription_FreeDataSubscriptionRcWithFlagData(&p->rc_with_flag_data);
    DataSubscription_FreeDataSubscriptionStatusFlight(&p->status_flight);
    DataSubscription_FreeDataSubscriptionStatusDisplayMode(&p->status_displaymode);
    DataSubscription_FreeDataSubscriptionControlDevice(&p->control_device);
    DataSubscription_FreeDataSubscriptionVector3i(&p->compass);
    DataSubscription_FreeDataSubscriptionFlightAnomaly(&p->flight_anomaly);
    DataSubscription_FreeDataSubscriptionAvoidData(&p->avoid_data);
    DataSubscription_FreeDataSubscriptionHomePointSetStatus(&p->home_point_set_status);
    DataSubscription_FreeDataSubscriptionHomePointInfo(&p->home_point_info);
    DataSubscription_FreeDataSubscriptionBatterySingleInfo(&p->battery_single_info_index1);
    DataSubscription_FreeDataSubscriptionBatterySingleInfo(&p->battery_single_info_index2);
    DataSubscription_FreeDataSubscriptionGimbalAngles(&p->gimbal_angles);
}

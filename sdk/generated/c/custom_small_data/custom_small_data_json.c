/**
 * @file    custom_small_data_json.c
 */
#include "custom_small_data_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode CustomSmallDataJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode CustomSmallDataJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *CustomSmallDataJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *CustomSmallDataJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *CustomSmallDataJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * TransmitRequest
 * ============================================================ */

ThingModelJsonObj_t *CustomSmallDataJson_EncodeTransmitRequest_AsObj(const T_TransmitRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "target", (int64_t)p->target);
    if (p->payload.data && p->payload.len > 0) {
        char *_hex = ThingModelHex_Encode(p->payload.data, p->payload.len);
        if (_hex) { ThingModelJson_AddString(root, "payload", _hex); ThingModelMem_Free(_hex); }
    }
    return root;
}

char *CustomSmallDataJson_EncodeTransmitRequest(const T_TransmitRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = CustomSmallDataJson_EncodeTransmitRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitRequest_FromObj(const ThingModelJsonObj_t *src, T_TransmitRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->target = (E_ChannelAddress)ThingModelJson_LookupInt(src, "target", 0);
    {
        const char *_hex = ThingModelJson_LookupString(src, "payload", NULL);
        if (_hex) ThingModelHex_Decode(_hex, &out->payload.data, &out->payload.len);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitRequest(const char *json, T_TransmitRequest *out)
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
    T_ThingModelReturnCode rc = CustomSmallDataJson_DecodeTransmitRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void CustomSmallData_FreeTransmitRequest(T_TransmitRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->payload.data); p->payload.data = NULL; p->payload.len = 0;
}

/* ============================================================
 * TransmitEventPush
 * ============================================================ */

ThingModelJsonObj_t *CustomSmallDataJson_EncodeTransmitEventPush_AsObj(const T_TransmitEventPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "source", (int64_t)p->source);
    if (p->payload.data && p->payload.len > 0) {
        char *_hex = ThingModelHex_Encode(p->payload.data, p->payload.len);
        if (_hex) { ThingModelJson_AddString(root, "payload", _hex); ThingModelMem_Free(_hex); }
    }
    return root;
}

char *CustomSmallDataJson_EncodeTransmitEventPush(const T_TransmitEventPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = CustomSmallDataJson_EncodeTransmitEventPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitEventPush_FromObj(const ThingModelJsonObj_t *src, T_TransmitEventPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->source = (E_ChannelAddress)ThingModelJson_LookupInt(src, "source", 0);
    {
        const char *_hex = ThingModelJson_LookupString(src, "payload", NULL);
        if (_hex) ThingModelHex_Decode(_hex, &out->payload.data, &out->payload.len);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitEventPush(const char *json, T_TransmitEventPush *out)
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
    T_ThingModelReturnCode rc = CustomSmallDataJson_DecodeTransmitEventPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void CustomSmallData_FreeTransmitEventPush(T_TransmitEventPush *p)
{
    if (!p) return;
    ThingModelMem_Free(p->payload.data); p->payload.data = NULL; p->payload.len = 0;
}

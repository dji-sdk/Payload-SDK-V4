/**
 * @file    mop_channel_json.c
 */
#include "mop_channel_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode MopChannelJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode MopChannelJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *MopChannelJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *MopChannelJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *MopChannelJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * MopEnableRequest
 * ============================================================ */

ThingModelJsonObj_t *MopChannelJson_EncodeMopEnableRequest_AsObj(const T_MopEnableRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "role", (int64_t)p->role);
    ThingModelJson_AddInt(root, "reliability", (int64_t)p->reliability);
    ThingModelJson_AddUInt(root, "channel_id", (uint64_t)p->channel_id);
    ThingModelJson_AddInt(root, "target_device_type", (int64_t)p->target_device_type);
    ThingModelJson_AddUInt(root, "target_address", (uint64_t)p->target_address);
    return root;
}

char *MopChannelJson_EncodeMopEnableRequest(const T_MopEnableRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = MopChannelJson_EncodeMopEnableRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopEnableRequest_FromObj(const ThingModelJsonObj_t *src, T_MopEnableRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->role = (E_Role)ThingModelJson_LookupInt(src, "role", 0);
    out->reliability = (E_Reliability)ThingModelJson_LookupInt(src, "reliability", 0);
    out->channel_id = (uint32_t)ThingModelJson_LookupInt(src, "channel_id", 0);
    out->target_device_type = (E_TargetDeviceType)ThingModelJson_LookupInt(src, "target_device_type", 0);
    out->target_address = (uint32_t)ThingModelJson_LookupInt(src, "target_address", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopEnableRequest(const char *json, T_MopEnableRequest *out)
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
    T_ThingModelReturnCode rc = MopChannelJson_DecodeMopEnableRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void MopChannel_FreeMopEnableRequest(T_MopEnableRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * MopEnableReply
 * ============================================================ */

ThingModelJsonObj_t *MopChannelJson_EncodeMopEnableReply_AsObj(const T_MopEnableReply *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "result", (int64_t)p->result);
    if (p->result_message) ThingModelJson_AddString(root, "result_message", p->result_message);
    ThingModelJson_AddUInt(root, "channel_id", (uint64_t)p->channel_id);
    if (p->state) ThingModelJson_AddString(root, "state", p->state);
    if (p->ip) ThingModelJson_AddString(root, "ip", p->ip);
    ThingModelJson_AddUInt(root, "port", (uint64_t)p->port);
    return root;
}

char *MopChannelJson_EncodeMopEnableReply(const T_MopEnableReply *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = MopChannelJson_EncodeMopEnableReply_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopEnableReply_FromObj(const ThingModelJsonObj_t *src, T_MopEnableReply *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->result = (int32_t)ThingModelJson_LookupInt(src, "result", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "result_message", NULL);
        if (_s) out->result_message = ThingModelMem_Strdup(_s);
    }
    out->channel_id = (uint32_t)ThingModelJson_LookupInt(src, "channel_id", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "state", NULL);
        if (_s) out->state = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "ip", NULL);
        if (_s) out->ip = ThingModelMem_Strdup(_s);
    }
    out->port = (uint32_t)ThingModelJson_LookupInt(src, "port", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopEnableReply(const char *json, T_MopEnableReply *out)
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
    T_ThingModelReturnCode rc = MopChannelJson_DecodeMopEnableReply_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void MopChannel_FreeMopEnableReply(T_MopEnableReply *p)
{
    if (!p) return;
    ThingModelMem_Free(p->result_message); p->result_message = NULL;
    ThingModelMem_Free(p->state); p->state = NULL;
    ThingModelMem_Free(p->ip); p->ip = NULL;
}

/* ============================================================
 * MopDisableRequest
 * ============================================================ */

ThingModelJsonObj_t *MopChannelJson_EncodeMopDisableRequest_AsObj(const T_MopDisableRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddUInt(root, "channel_id", (uint64_t)p->channel_id);
    return root;
}

char *MopChannelJson_EncodeMopDisableRequest(const T_MopDisableRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = MopChannelJson_EncodeMopDisableRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopDisableRequest_FromObj(const ThingModelJsonObj_t *src, T_MopDisableRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->channel_id = (uint32_t)ThingModelJson_LookupInt(src, "channel_id", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode MopChannelJson_DecodeMopDisableRequest(const char *json, T_MopDisableRequest *out)
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
    T_ThingModelReturnCode rc = MopChannelJson_DecodeMopDisableRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void MopChannel_FreeMopDisableRequest(T_MopDisableRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * ChannelEventPush
 * ============================================================ */

ThingModelJsonObj_t *MopChannelJson_EncodeChannelEventPush_AsObj(const T_ChannelEventPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddUInt(root, "channel_id", (uint64_t)p->channel_id);
    ThingModelJson_AddInt(root, "state", (int64_t)p->state);
    return root;
}

char *MopChannelJson_EncodeChannelEventPush(const T_ChannelEventPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = MopChannelJson_EncodeChannelEventPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode MopChannelJson_DecodeChannelEventPush_FromObj(const ThingModelJsonObj_t *src, T_ChannelEventPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->channel_id = (uint32_t)ThingModelJson_LookupInt(src, "channel_id", 0);
    out->state = (E_ChannelState)ThingModelJson_LookupInt(src, "state", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode MopChannelJson_DecodeChannelEventPush(const char *json, T_ChannelEventPush *out)
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
    T_ThingModelReturnCode rc = MopChannelJson_DecodeChannelEventPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void MopChannel_FreeChannelEventPush(T_ChannelEventPush *p)
{
    if (!p) return;
}

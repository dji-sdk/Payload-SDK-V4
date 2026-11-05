/**
 * @file    time_sync_json.c
 */
#include "time_sync_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode TimeSyncJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode TimeSyncJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *TimeSyncJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *TimeSyncJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *TimeSyncJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * TimeSyncSetSignalModeRequest
 * ============================================================ */

ThingModelJsonObj_t *TimeSyncJson_EncodeTimeSyncSetSignalModeRequest_AsObj(const T_TimeSyncSetSignalModeRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "mode", (int64_t)p->mode);
    return root;
}

char *TimeSyncJson_EncodeTimeSyncSetSignalModeRequest(const T_TimeSyncSetSignalModeRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = TimeSyncJson_EncodeTimeSyncSetSignalModeRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncSetSignalModeRequest_FromObj(const ThingModelJsonObj_t *src, T_TimeSyncSetSignalModeRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->mode = (E_TimeSyncSignalMode)ThingModelJson_LookupInt(src, "mode", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncSetSignalModeRequest(const char *json, T_TimeSyncSetSignalModeRequest *out)
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
    T_ThingModelReturnCode rc = TimeSyncJson_DecodeTimeSyncSetSignalModeRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void TimeSync_FreeTimeSyncSetSignalModeRequest(T_TimeSyncSetSignalModeRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * TimeSyncTimeSyncPush
 * ============================================================ */

ThingModelJsonObj_t *TimeSyncJson_EncodeTimeSyncTimeSyncPush_AsObj(const T_TimeSyncTimeSyncPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "year", (int64_t)p->year);
    ThingModelJson_AddInt(root, "month", (int64_t)p->month);
    ThingModelJson_AddInt(root, "day", (int64_t)p->day);
    ThingModelJson_AddInt(root, "hour", (int64_t)p->hour);
    ThingModelJson_AddInt(root, "minute", (int64_t)p->minute);
    ThingModelJson_AddInt(root, "second", (int64_t)p->second);
    ThingModelJson_AddInt(root, "nano_second", (int64_t)p->nano_second);
    return root;
}

char *TimeSyncJson_EncodeTimeSyncTimeSyncPush(const T_TimeSyncTimeSyncPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = TimeSyncJson_EncodeTimeSyncTimeSyncPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncTimeSyncPush_FromObj(const ThingModelJsonObj_t *src, T_TimeSyncTimeSyncPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->year = (int32_t)ThingModelJson_LookupInt(src, "year", 0);
    out->month = (int32_t)ThingModelJson_LookupInt(src, "month", 0);
    out->day = (int32_t)ThingModelJson_LookupInt(src, "day", 0);
    out->hour = (int32_t)ThingModelJson_LookupInt(src, "hour", 0);
    out->minute = (int32_t)ThingModelJson_LookupInt(src, "minute", 0);
    out->second = (int32_t)ThingModelJson_LookupInt(src, "second", 0);
    out->nano_second = (int32_t)ThingModelJson_LookupInt(src, "nano_second", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncTimeSyncPush(const char *json, T_TimeSyncTimeSyncPush *out)
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
    T_ThingModelReturnCode rc = TimeSyncJson_DecodeTimeSyncTimeSyncPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void TimeSync_FreeTimeSyncTimeSyncPush(T_TimeSyncTimeSyncPush *p)
{
    if (!p) return;
}

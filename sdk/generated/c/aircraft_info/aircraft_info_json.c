/**
 * @file    aircraft_info_json.c
 */
#include "aircraft_info_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode AircraftInfoJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode AircraftInfoJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *AircraftInfoJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *AircraftInfoJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *AircraftInfoJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * MobileAppInfo
 * ============================================================ */

ThingModelJsonObj_t *AircraftInfoJson_EncodeMobileAppInfo_AsObj(const T_MobileAppInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "language", (int64_t)p->language);
    ThingModelJson_AddInt(root, "screen_type", (int64_t)p->screen_type);
    return root;
}

char *AircraftInfoJson_EncodeMobileAppInfo(const T_MobileAppInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = AircraftInfoJson_EncodeMobileAppInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeMobileAppInfo_FromObj(const ThingModelJsonObj_t *src, T_MobileAppInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->language = (int32_t)ThingModelJson_LookupInt(src, "language", 0);
    out->screen_type = (int32_t)ThingModelJson_LookupInt(src, "screen_type", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeMobileAppInfo(const char *json, T_MobileAppInfo *out)
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
    T_ThingModelReturnCode rc = AircraftInfoJson_DecodeMobileAppInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void AircraftInfo_FreeMobileAppInfo(T_MobileAppInfo *p)
{
    if (!p) return;
}

/* ============================================================
 * AircraftDeviceInfo
 * ============================================================ */

ThingModelJsonObj_t *AircraftInfoJson_EncodeAircraftDeviceInfo_AsObj(const T_AircraftDeviceInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->device_id) ThingModelJson_AddString(root, "device_id", p->device_id);
    if (p->device_name) ThingModelJson_AddString(root, "device_name", p->device_name);
    if (p->serial_number) ThingModelJson_AddString(root, "serial_number", p->serial_number);
    if (p->firmware_version) ThingModelJson_AddString(root, "firmware_version", p->firmware_version);
    ThingModelJson_AddInt(root, "device_type", (int64_t)p->device_type);
    return root;
}

char *AircraftInfoJson_EncodeAircraftDeviceInfo(const T_AircraftDeviceInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = AircraftInfoJson_EncodeAircraftDeviceInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftDeviceInfo_FromObj(const ThingModelJsonObj_t *src, T_AircraftDeviceInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "device_id", NULL);
        if (_s) out->device_id = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "device_name", NULL);
        if (_s) out->device_name = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "serial_number", NULL);
        if (_s) out->serial_number = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "firmware_version", NULL);
        if (_s) out->firmware_version = ThingModelMem_Strdup(_s);
    }
    out->device_type = (int32_t)ThingModelJson_LookupInt(src, "device_type", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftDeviceInfo(const char *json, T_AircraftDeviceInfo *out)
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
    T_ThingModelReturnCode rc = AircraftInfoJson_DecodeAircraftDeviceInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void AircraftInfo_FreeAircraftDeviceInfo(T_AircraftDeviceInfo *p)
{
    if (!p) return;
    ThingModelMem_Free(p->device_id); p->device_id = NULL;
    ThingModelMem_Free(p->device_name); p->device_name = NULL;
    ThingModelMem_Free(p->serial_number); p->serial_number = NULL;
    ThingModelMem_Free(p->firmware_version); p->firmware_version = NULL;
}

/* ============================================================
 * AircraftInfoPush
 * ============================================================ */

ThingModelJsonObj_t *AircraftInfoJson_EncodeAircraftInfoPush_AsObj(const T_AircraftInfoPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "aircraft_type", (int64_t)p->aircraft_type);
    if (p->aircraft_version) ThingModelJson_AddString(root, "aircraft_version", p->aircraft_version);
    ThingModelJson_AddObject(root, "mobile_app_info", AircraftInfoJson_EncodeMobileAppInfo_AsObj(&p->mobile_app_info));
    if (p->devices_info_count > 0 && p->devices_info) {
        ThingModelJsonObj_t *arr = ThingModelJson_NewArray();
        for (size_t i = 0; i < p->devices_info_count; i++) {
            ThingModelJson_ArrAppendObject(arr, AircraftInfoJson_EncodeAircraftDeviceInfo_AsObj(&p->devices_info[i]));
        }
        ThingModelJson_AddArray(root, "devices_info", arr);
    }
    return root;
}

char *AircraftInfoJson_EncodeAircraftInfoPush(const T_AircraftInfoPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = AircraftInfoJson_EncodeAircraftInfoPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftInfoPush_FromObj(const ThingModelJsonObj_t *src, T_AircraftInfoPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->aircraft_type = (int32_t)ThingModelJson_LookupInt(src, "aircraft_type", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "aircraft_version", NULL);
        if (_s) out->aircraft_version = ThingModelMem_Strdup(_s);
    }
    {
        const ThingModelJsonObj_t *_o = ThingModelJson_GetItem(src, "mobile_app_info");
        if (_o) AircraftInfoJson_DecodeMobileAppInfo_FromObj(_o, &out->mobile_app_info);
    }
    {
        const ThingModelJsonObj_t *_arr = ThingModelJson_GetItem(src, "devices_info");
        int _n = (_arr && ThingModelJson_IsArray(_arr)) ? ThingModelJson_GetArraySize(_arr) : 0;
        if (_n > 0) {
            out->devices_info = (T_AircraftDeviceInfo *)ThingModelMem_Calloc((size_t)_n, sizeof(T_AircraftDeviceInfo));
            if (out->devices_info) {
                for (int i = 0; i < _n; i++)
                    AircraftInfoJson_DecodeAircraftDeviceInfo_FromObj(ThingModelJson_GetArrayItem(_arr, i), &out->devices_info[i]);
                out->devices_info_count = (size_t)_n;
            }
        }
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftInfoPush(const char *json, T_AircraftInfoPush *out)
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
    T_ThingModelReturnCode rc = AircraftInfoJson_DecodeAircraftInfoPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void AircraftInfo_FreeAircraftInfoPush(T_AircraftInfoPush *p)
{
    if (!p) return;
    ThingModelMem_Free(p->aircraft_version); p->aircraft_version = NULL;
    AircraftInfo_FreeMobileAppInfo(&p->mobile_app_info);
    if (p->devices_info) {
        for (size_t i = 0; i < p->devices_info_count; i++) {
            AircraftInfo_FreeAircraftDeviceInfo(&p->devices_info[i]);
        }
        ThingModelMem_Free(p->devices_info);
        p->devices_info = NULL;
    }
    p->devices_info_count = 0;
}

/**
 * @file    device_info_json.c
 */
#include "device_info_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode DeviceInfoJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode DeviceInfoJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *DeviceInfoJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *DeviceInfoJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *DeviceInfoJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * DeviceInfoPush
 * ============================================================ */

ThingModelJsonObj_t *DeviceInfoJson_EncodeDeviceInfoPush_AsObj(const T_DeviceInfoPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->device_id) ThingModelJson_AddString(root, "device_id", p->device_id);
    if (p->device_name) ThingModelJson_AddString(root, "device_name", p->device_name);
    if (p->serial_number) ThingModelJson_AddString(root, "serial_number", p->serial_number);
    if (p->firmware_version) ThingModelJson_AddString(root, "firmware_version", p->firmware_version);
    ThingModelJson_AddInt(root, "device_type", (int64_t)p->device_type);
    if (p->capabilities_count > 0 && p->capabilities) {
        ThingModelJsonObj_t *arr = ThingModelJson_NewArray();
        for (size_t i = 0; i < p->capabilities_count; i++) {
            ThingModelJson_ArrAppendString(arr, p->capabilities[i]);
        }
        ThingModelJson_AddArray(root, "capabilities", arr);
    }
    return root;
}

char *DeviceInfoJson_EncodeDeviceInfoPush(const T_DeviceInfoPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = DeviceInfoJson_EncodeDeviceInfoPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode DeviceInfoJson_DecodeDeviceInfoPush_FromObj(const ThingModelJsonObj_t *src, T_DeviceInfoPush *out)
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
    {
        const ThingModelJsonObj_t *_arr = ThingModelJson_GetItem(src, "capabilities");
        int _n = (_arr && ThingModelJson_IsArray(_arr)) ? ThingModelJson_GetArraySize(_arr) : 0;
        if (_n > 0) {
            out->capabilities = (char **)ThingModelMem_Calloc((size_t)_n, sizeof(char *));
            if (out->capabilities) {
                for (int i = 0; i < _n; i++) {
                    const char *_s = ThingModelJson_GetString(ThingModelJson_GetArrayItem(_arr, i), "");
                    out->capabilities[i] = ThingModelMem_Strdup(_s ? _s : "");
                }
                out->capabilities_count = (size_t)_n;
            }
        }
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode DeviceInfoJson_DecodeDeviceInfoPush(const char *json, T_DeviceInfoPush *out)
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
    T_ThingModelReturnCode rc = DeviceInfoJson_DecodeDeviceInfoPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void DeviceInfo_FreeDeviceInfoPush(T_DeviceInfoPush *p)
{
    if (!p) return;
    ThingModelMem_Free(p->device_id); p->device_id = NULL;
    ThingModelMem_Free(p->device_name); p->device_name = NULL;
    ThingModelMem_Free(p->serial_number); p->serial_number = NULL;
    ThingModelMem_Free(p->firmware_version); p->firmware_version = NULL;
    if (p->capabilities) {
        for (size_t i = 0; i < p->capabilities_count; i++) {
            ThingModelMem_Free(p->capabilities[i]);
        }
        ThingModelMem_Free(p->capabilities);
        p->capabilities = NULL;
    }
    p->capabilities_count = 0;
}

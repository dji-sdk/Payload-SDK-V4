/**
 * @file    upgrade_json.c
 */
#include "upgrade_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode UpgradeJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode UpgradeJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *UpgradeJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *UpgradeJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *UpgradeJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * UpgradeEnterModeReply
 * ============================================================ */

ThingModelJsonObj_t *UpgradeJson_EncodeUpgradeEnterModeReply_AsObj(const T_UpgradeEnterModeReply *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "result", (int64_t)p->result);
    if (p->result_message) ThingModelJson_AddString(root, "result_message", p->result_message);
    ThingModelJson_AddInt(root, "wait_time", (int64_t)p->wait_time);
    if (p->url) ThingModelJson_AddString(root, "url", p->url);
    return root;
}

char *UpgradeJson_EncodeUpgradeEnterModeReply(const T_UpgradeEnterModeReply *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = UpgradeJson_EncodeUpgradeEnterModeReply_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode UpgradeJson_DecodeUpgradeEnterModeReply_FromObj(const ThingModelJsonObj_t *src, T_UpgradeEnterModeReply *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->result = (int32_t)ThingModelJson_LookupInt(src, "result", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "result_message", NULL);
        if (_s) out->result_message = ThingModelMem_Strdup(_s);
    }
    out->wait_time = (int32_t)ThingModelJson_LookupInt(src, "wait_time", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "url", NULL);
        if (_s) out->url = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode UpgradeJson_DecodeUpgradeEnterModeReply(const char *json, T_UpgradeEnterModeReply *out)
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
    T_ThingModelReturnCode rc = UpgradeJson_DecodeUpgradeEnterModeReply_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Upgrade_FreeUpgradeEnterModeReply(T_UpgradeEnterModeReply *p)
{
    if (!p) return;
    ThingModelMem_Free(p->result_message); p->result_message = NULL;
    ThingModelMem_Free(p->url); p->url = NULL;
}

/* ============================================================
 * UpgradeStatePush
 * ============================================================ */

ThingModelJsonObj_t *UpgradeJson_EncodeUpgradeStatePush_AsObj(const T_UpgradeStatePush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "upgrade_stage", (int64_t)p->upgrade_stage);
    ThingModelJson_AddInt(root, "value", (int64_t)p->value);
    return root;
}

char *UpgradeJson_EncodeUpgradeStatePush(const T_UpgradeStatePush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = UpgradeJson_EncodeUpgradeStatePush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode UpgradeJson_DecodeUpgradeStatePush_FromObj(const ThingModelJsonObj_t *src, T_UpgradeStatePush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->upgrade_stage = (int32_t)ThingModelJson_LookupInt(src, "upgrade_stage", 0);
    out->value = (int32_t)ThingModelJson_LookupInt(src, "value", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode UpgradeJson_DecodeUpgradeStatePush(const char *json, T_UpgradeStatePush *out)
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
    T_ThingModelReturnCode rc = UpgradeJson_DecodeUpgradeStatePush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Upgrade_FreeUpgradeStatePush(T_UpgradeStatePush *p)
{
    if (!p) return;
}

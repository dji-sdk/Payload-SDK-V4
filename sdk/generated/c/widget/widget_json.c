/**
 * @file    widget_json.c
 */
#include "widget_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode WidgetJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode WidgetJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *WidgetJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *WidgetJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *WidgetJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * WidgetFileInfo
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetFileInfo_AsObj(const T_WidgetFileInfo *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "file_index", (int64_t)p->file_index);
    if (p->file_name) ThingModelJson_AddString(root, "file_name", p->file_name);
    ThingModelJson_AddInt(root, "file_size", (int64_t)p->file_size);
    if (p->md5) ThingModelJson_AddString(root, "md5", p->md5);
    return root;
}

char *WidgetJson_EncodeWidgetFileInfo(const T_WidgetFileInfo *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetFileInfo_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetFileInfo_FromObj(const ThingModelJsonObj_t *src, T_WidgetFileInfo *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->file_index = (int32_t)ThingModelJson_LookupInt(src, "file_index", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "file_name", NULL);
        if (_s) out->file_name = ThingModelMem_Strdup(_s);
    }
    out->file_size = (int32_t)ThingModelJson_LookupInt(src, "file_size", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "md5", NULL);
        if (_s) out->md5 = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetFileInfo(const char *json, T_WidgetFileInfo *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetFileInfo_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetFileInfo(T_WidgetFileInfo *p)
{
    if (!p) return;
    ThingModelMem_Free(p->file_name); p->file_name = NULL;
    ThingModelMem_Free(p->md5); p->md5 = NULL;
}

/* ============================================================
 * WidgetGetFileListRequest
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetGetFileListRequest_AsObj(const T_WidgetGetFileListRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "language", (int64_t)p->language);
    ThingModelJson_AddInt(root, "screen_type", (int64_t)p->screen_type);
    return root;
}

char *WidgetJson_EncodeWidgetGetFileListRequest(const T_WidgetGetFileListRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetGetFileListRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListRequest_FromObj(const ThingModelJsonObj_t *src, T_WidgetGetFileListRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->language = (E_AppLanguage)ThingModelJson_LookupInt(src, "language", 0);
    out->screen_type = (E_ScreenType)ThingModelJson_LookupInt(src, "screen_type", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListRequest(const char *json, T_WidgetGetFileListRequest *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetGetFileListRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetGetFileListRequest(T_WidgetGetFileListRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * WidgetGetFileListReply
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetGetFileListReply_AsObj(const T_WidgetGetFileListReply *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "result", (int64_t)p->result);
    if (p->result_message) ThingModelJson_AddString(root, "result_message", p->result_message);
    if (p->file_list_count > 0 && p->file_list) {
        ThingModelJsonObj_t *arr = ThingModelJson_NewArray();
        for (size_t i = 0; i < p->file_list_count; i++) {
            ThingModelJson_ArrAppendObject(arr, WidgetJson_EncodeWidgetFileInfo_AsObj(&p->file_list[i]));
        }
        ThingModelJson_AddArray(root, "file_list", arr);
    }
    return root;
}

char *WidgetJson_EncodeWidgetGetFileListReply(const T_WidgetGetFileListReply *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetGetFileListReply_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListReply_FromObj(const ThingModelJsonObj_t *src, T_WidgetGetFileListReply *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->result = (int32_t)ThingModelJson_LookupInt(src, "result", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "result_message", NULL);
        if (_s) out->result_message = ThingModelMem_Strdup(_s);
    }
    {
        const ThingModelJsonObj_t *_arr = ThingModelJson_GetItem(src, "file_list");
        int _n = (_arr && ThingModelJson_IsArray(_arr)) ? ThingModelJson_GetArraySize(_arr) : 0;
        if (_n > 0) {
            out->file_list = (T_WidgetFileInfo *)ThingModelMem_Calloc((size_t)_n, sizeof(T_WidgetFileInfo));
            if (out->file_list) {
                for (int i = 0; i < _n; i++)
                    WidgetJson_DecodeWidgetFileInfo_FromObj(ThingModelJson_GetArrayItem(_arr, i), &out->file_list[i]);
                out->file_list_count = (size_t)_n;
            }
        }
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListReply(const char *json, T_WidgetGetFileListReply *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetGetFileListReply_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetGetFileListReply(T_WidgetGetFileListReply *p)
{
    if (!p) return;
    ThingModelMem_Free(p->result_message); p->result_message = NULL;
    if (p->file_list) {
        for (size_t i = 0; i < p->file_list_count; i++) {
            Widget_FreeWidgetFileInfo(&p->file_list[i]);
        }
        ThingModelMem_Free(p->file_list);
        p->file_list = NULL;
    }
    p->file_list_count = 0;
}

/* ============================================================
 * WidgetSetValueRequest
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetSetValueRequest_AsObj(const T_WidgetSetValueRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "widget_index", (int64_t)p->widget_index);
    ThingModelJson_AddInt(root, "widget_type", (int64_t)p->widget_type);
    ThingModelJson_AddInt(root, "widget_value", (int64_t)p->widget_value);
    return root;
}

char *WidgetJson_EncodeWidgetSetValueRequest(const T_WidgetSetValueRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetSetValueRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetSetValueRequest_FromObj(const ThingModelJsonObj_t *src, T_WidgetSetValueRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->widget_index = (int32_t)ThingModelJson_LookupInt(src, "widget_index", 0);
    out->widget_type = (int32_t)ThingModelJson_LookupInt(src, "widget_type", 0);
    out->widget_value = (int32_t)ThingModelJson_LookupInt(src, "widget_value", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetSetValueRequest(const char *json, T_WidgetSetValueRequest *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetSetValueRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetSetValueRequest(T_WidgetSetValueRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * WidgetGetValueRequest
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetGetValueRequest_AsObj(const T_WidgetGetValueRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "widget_index", (int64_t)p->widget_index);
    return root;
}

char *WidgetJson_EncodeWidgetGetValueRequest(const T_WidgetGetValueRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetGetValueRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueRequest_FromObj(const ThingModelJsonObj_t *src, T_WidgetGetValueRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->widget_index = (int32_t)ThingModelJson_LookupInt(src, "widget_index", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueRequest(const char *json, T_WidgetGetValueRequest *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetGetValueRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetGetValueRequest(T_WidgetGetValueRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * WidgetGetValueReply
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetGetValueReply_AsObj(const T_WidgetGetValueReply *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "result", (int64_t)p->result);
    if (p->result_message) ThingModelJson_AddString(root, "result_message", p->result_message);
    ThingModelJson_AddInt(root, "widget_index", (int64_t)p->widget_index);
    ThingModelJson_AddInt(root, "widget_value", (int64_t)p->widget_value);
    return root;
}

char *WidgetJson_EncodeWidgetGetValueReply(const T_WidgetGetValueReply *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetGetValueReply_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueReply_FromObj(const ThingModelJsonObj_t *src, T_WidgetGetValueReply *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->result = (int32_t)ThingModelJson_LookupInt(src, "result", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "result_message", NULL);
        if (_s) out->result_message = ThingModelMem_Strdup(_s);
    }
    out->widget_index = (int32_t)ThingModelJson_LookupInt(src, "widget_index", 0);
    out->widget_value = (int32_t)ThingModelJson_LookupInt(src, "widget_value", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueReply(const char *json, T_WidgetGetValueReply *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetGetValueReply_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetGetValueReply(T_WidgetGetValueReply *p)
{
    if (!p) return;
    ThingModelMem_Free(p->result_message); p->result_message = NULL;
}

/* ============================================================
 * WidgetStateItem
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetStateItem_AsObj(const T_WidgetStateItem *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "widget_index", (int64_t)p->widget_index);
    if (p->widget_type) ThingModelJson_AddString(root, "widget_type", p->widget_type);
    ThingModelJson_AddInt(root, "widget_value", (int64_t)p->widget_value);
    return root;
}

char *WidgetJson_EncodeWidgetStateItem(const T_WidgetStateItem *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetStateItem_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetStateItem_FromObj(const ThingModelJsonObj_t *src, T_WidgetStateItem *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->widget_index = (int32_t)ThingModelJson_LookupInt(src, "widget_index", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "widget_type", NULL);
        if (_s) out->widget_type = ThingModelMem_Strdup(_s);
    }
    out->widget_value = (int32_t)ThingModelJson_LookupInt(src, "widget_value", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetStateItem(const char *json, T_WidgetStateItem *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetStateItem_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetStateItem(T_WidgetStateItem *p)
{
    if (!p) return;
    ThingModelMem_Free(p->widget_type); p->widget_type = NULL;
}

/* ============================================================
 * WidgetStatePush
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetStatePush_AsObj(const T_WidgetStatePush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->widget_list_count > 0 && p->widget_list) {
        ThingModelJsonObj_t *arr = ThingModelJson_NewArray();
        for (size_t i = 0; i < p->widget_list_count; i++) {
            ThingModelJson_ArrAppendObject(arr, WidgetJson_EncodeWidgetStateItem_AsObj(&p->widget_list[i]));
        }
        ThingModelJson_AddArray(root, "widget_list", arr);
    }
    return root;
}

char *WidgetJson_EncodeWidgetStatePush(const T_WidgetStatePush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetStatePush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetStatePush_FromObj(const ThingModelJsonObj_t *src, T_WidgetStatePush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const ThingModelJsonObj_t *_arr = ThingModelJson_GetItem(src, "widget_list");
        int _n = (_arr && ThingModelJson_IsArray(_arr)) ? ThingModelJson_GetArraySize(_arr) : 0;
        if (_n > 0) {
            out->widget_list = (T_WidgetStateItem *)ThingModelMem_Calloc((size_t)_n, sizeof(T_WidgetStateItem));
            if (out->widget_list) {
                for (int i = 0; i < _n; i++)
                    WidgetJson_DecodeWidgetStateItem_FromObj(ThingModelJson_GetArrayItem(_arr, i), &out->widget_list[i]);
                out->widget_list_count = (size_t)_n;
            }
        }
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetStatePush(const char *json, T_WidgetStatePush *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetStatePush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetStatePush(T_WidgetStatePush *p)
{
    if (!p) return;
    if (p->widget_list) {
        for (size_t i = 0; i < p->widget_list_count; i++) {
            Widget_FreeWidgetStateItem(&p->widget_list[i]);
        }
        ThingModelMem_Free(p->widget_list);
        p->widget_list = NULL;
    }
    p->widget_list_count = 0;
}

/* ============================================================
 * WidgetFloatWindowPush
 * ============================================================ */

ThingModelJsonObj_t *WidgetJson_EncodeWidgetFloatWindowPush_AsObj(const T_WidgetFloatWindowPush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->message) ThingModelJson_AddString(root, "message", p->message);
    return root;
}

char *WidgetJson_EncodeWidgetFloatWindowPush(const T_WidgetFloatWindowPush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = WidgetJson_EncodeWidgetFloatWindowPush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetFloatWindowPush_FromObj(const ThingModelJsonObj_t *src, T_WidgetFloatWindowPush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "message", NULL);
        if (_s) out->message = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode WidgetJson_DecodeWidgetFloatWindowPush(const char *json, T_WidgetFloatWindowPush *out)
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
    T_ThingModelReturnCode rc = WidgetJson_DecodeWidgetFloatWindowPush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Widget_FreeWidgetFloatWindowPush(T_WidgetFloatWindowPush *p)
{
    if (!p) return;
    ThingModelMem_Free(p->message); p->message = NULL;
}

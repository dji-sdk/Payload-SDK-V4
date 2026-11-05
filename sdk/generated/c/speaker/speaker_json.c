/**
 * @file    speaker_json.c
 */
#include "speaker_json.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/* common reply                                                 */
/* ============================================================ */

T_ThingModelReturnCode SpeakerJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out)
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

T_ThingModelReturnCode SpeakerJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out)
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

char *SpeakerJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json)
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

char *SpeakerJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props)
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

char *SpeakerJson_EncodePropertySetRaw(uint32_t seq, const char *data_json)
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
 * SpeakerSetWorkModeRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerSetWorkModeRequest_AsObj(const T_SpeakerSetWorkModeRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "work_mode", (int64_t)p->work_mode);
    return root;
}

char *SpeakerJson_EncodeSpeakerSetWorkModeRequest(const T_SpeakerSetWorkModeRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerSetWorkModeRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetWorkModeRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerSetWorkModeRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->work_mode = (int32_t)ThingModelJson_LookupInt(src, "work_mode", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetWorkModeRequest(const char *json, T_SpeakerSetWorkModeRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerSetWorkModeRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerSetWorkModeRequest(T_SpeakerSetWorkModeRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * SpeakerSetPlayModeRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerSetPlayModeRequest_AsObj(const T_SpeakerSetPlayModeRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "play_mode", (int64_t)p->play_mode);
    return root;
}

char *SpeakerJson_EncodeSpeakerSetPlayModeRequest(const T_SpeakerSetPlayModeRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerSetPlayModeRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetPlayModeRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerSetPlayModeRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->play_mode = (int32_t)ThingModelJson_LookupInt(src, "play_mode", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetPlayModeRequest(const char *json, T_SpeakerSetPlayModeRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerSetPlayModeRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerSetPlayModeRequest(T_SpeakerSetPlayModeRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * SpeakerSetVolumeRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerSetVolumeRequest_AsObj(const T_SpeakerSetVolumeRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "volume", (int64_t)p->volume);
    return root;
}

char *SpeakerJson_EncodeSpeakerSetVolumeRequest(const T_SpeakerSetVolumeRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerSetVolumeRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetVolumeRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerSetVolumeRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->volume = (int32_t)ThingModelJson_LookupInt(src, "volume", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetVolumeRequest(const char *json, T_SpeakerSetVolumeRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerSetVolumeRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerSetVolumeRequest(T_SpeakerSetVolumeRequest *p)
{
    if (!p) return;
}

/* ============================================================
 * SpeakerTransStartRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerTransStartRequest_AsObj(const T_SpeakerTransStartRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "file_type", (int64_t)p->file_type);
    if (p->uuid) ThingModelJson_AddString(root, "uuid", p->uuid);
    if (p->file_name) ThingModelJson_AddString(root, "file_name", p->file_name);
    ThingModelJson_AddInt(root, "decode_bitrate", (int64_t)p->decode_bitrate);
    return root;
}

char *SpeakerJson_EncodeSpeakerTransStartRequest(const T_SpeakerTransStartRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerTransStartRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransStartRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerTransStartRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->file_type = (int32_t)ThingModelJson_LookupInt(src, "file_type", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "uuid", NULL);
        if (_s) out->uuid = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "file_name", NULL);
        if (_s) out->file_name = ThingModelMem_Strdup(_s);
    }
    out->decode_bitrate = (int32_t)ThingModelJson_LookupInt(src, "decode_bitrate", 0);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransStartRequest(const char *json, T_SpeakerTransStartRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerTransStartRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerTransStartRequest(T_SpeakerTransStartRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->uuid); p->uuid = NULL;
    ThingModelMem_Free(p->file_name); p->file_name = NULL;
}

/* ============================================================
 * SpeakerTransDataRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerTransDataRequest_AsObj(const T_SpeakerTransDataRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "packet_index", (int64_t)p->packet_index);
    ThingModelJson_AddInt(root, "offset", (int64_t)p->offset);
    if (p->data.data && p->data.len > 0) {
        char *_hex = ThingModelHex_Encode(p->data.data, p->data.len);
        if (_hex) { ThingModelJson_AddString(root, "data", _hex); ThingModelMem_Free(_hex); }
    }
    return root;
}

char *SpeakerJson_EncodeSpeakerTransDataRequest(const T_SpeakerTransDataRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerTransDataRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransDataRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerTransDataRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->packet_index = (int32_t)ThingModelJson_LookupInt(src, "packet_index", 0);
    out->offset = (int32_t)ThingModelJson_LookupInt(src, "offset", 0);
    {
        const char *_hex = ThingModelJson_LookupString(src, "data", NULL);
        if (_hex) ThingModelHex_Decode(_hex, &out->data.data, &out->data.len);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransDataRequest(const char *json, T_SpeakerTransDataRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerTransDataRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerTransDataRequest(T_SpeakerTransDataRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->data.data); p->data.data = NULL; p->data.len = 0;
}

/* ============================================================
 * SpeakerTransFinishRequest
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerTransFinishRequest_AsObj(const T_SpeakerTransFinishRequest *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    if (p->md5) ThingModelJson_AddString(root, "md5", p->md5);
    return root;
}

char *SpeakerJson_EncodeSpeakerTransFinishRequest(const T_SpeakerTransFinishRequest *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerTransFinishRequest_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransFinishRequest_FromObj(const ThingModelJsonObj_t *src, T_SpeakerTransFinishRequest *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    {
        const char *_s = ThingModelJson_LookupString(src, "md5", NULL);
        if (_s) out->md5 = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransFinishRequest(const char *json, T_SpeakerTransFinishRequest *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerTransFinishRequest_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerTransFinishRequest(T_SpeakerTransFinishRequest *p)
{
    if (!p) return;
    ThingModelMem_Free(p->md5); p->md5 = NULL;
}

/* ============================================================
 * SpeakerStatePush
 * ============================================================ */

ThingModelJsonObj_t *SpeakerJson_EncodeSpeakerStatePush_AsObj(const T_SpeakerStatePush *p)
{
    ThingModelJsonObj_t *root = ThingModelJson_NewObject();
    if (!root || !p) return root;
    ThingModelJson_AddInt(root, "system_state", (int64_t)p->system_state);
    ThingModelJson_AddInt(root, "work_mode", (int64_t)p->work_mode);
    ThingModelJson_AddInt(root, "play_mode", (int64_t)p->play_mode);
    ThingModelJson_AddInt(root, "play_volume", (int64_t)p->play_volume);
    if (p->play_file_name) ThingModelJson_AddString(root, "play_file_name", p->play_file_name);
    if (p->play_file_uuid) ThingModelJson_AddString(root, "play_file_uuid", p->play_file_uuid);
    return root;
}

char *SpeakerJson_EncodeSpeakerStatePush(const T_SpeakerStatePush *p)
{
    if (!p) return NULL;
    ThingModelJsonObj_t *root = SpeakerJson_EncodeSpeakerStatePush_AsObj(p);
    if (!root) return NULL;
    char *s = ThingModelJson_PrintCompact(root);
    ThingModelJson_Free(root);
    return s;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerStatePush_FromObj(const ThingModelJsonObj_t *src, T_SpeakerStatePush *out)
{
    if (!out) return THING_MODEL_RC_INVALID_ARG;
    /* out has already been zero-init'd/Free'd by the outer layer; if the caller reuses a struct with heap content, freeing it beforehand is the user's responsibility */
    if (!src) return THING_MODEL_RC_OK;
    out->system_state = (int32_t)ThingModelJson_LookupInt(src, "system_state", 0);
    out->work_mode = (int32_t)ThingModelJson_LookupInt(src, "work_mode", 0);
    out->play_mode = (int32_t)ThingModelJson_LookupInt(src, "play_mode", 0);
    out->play_volume = (int32_t)ThingModelJson_LookupInt(src, "play_volume", 0);
    {
        const char *_s = ThingModelJson_LookupString(src, "play_file_name", NULL);
        if (_s) out->play_file_name = ThingModelMem_Strdup(_s);
    }
    {
        const char *_s = ThingModelJson_LookupString(src, "play_file_uuid", NULL);
        if (_s) out->play_file_uuid = ThingModelMem_Strdup(_s);
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode SpeakerJson_DecodeSpeakerStatePush(const char *json, T_SpeakerStatePush *out)
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
    T_ThingModelReturnCode rc = SpeakerJson_DecodeSpeakerStatePush_FromObj(root, out);
    ThingModelJson_Free(r);
    return rc;
}

void Speaker_FreeSpeakerStatePush(T_SpeakerStatePush *p)
{
    if (!p) return;
    ThingModelMem_Free(p->play_file_name); p->play_file_name = NULL;
    ThingModelMem_Free(p->play_file_uuid); p->play_file_uuid = NULL;
}

/**
 * @file    mop_channel_json.h
 * @brief   MopChannel struct ↔ cJSON conversion.
 *
 * Conventions:
 *   Encode<M>(src)               → returns a malloc'd compact JSON string; the caller free()s it.
 *   Decode<M>(json, out)         → takes a JSON string, Parses it internally; fields written into *out; returns RC.
 *                                  The heap fields (string / bytes / repeated) in out are malloc'd by this function,
 *                                  and after use you must call <Cap>_Free<M>(out) to free them, otherwise memory leaks.
 *   <Cap>_Free<M>(p)             → recursively frees all heap fields inside *p; safe to call repeatedly (NULL+0 compatible);
 *                                  does not free p itself.
 *
 *   Encode<M>_AsObj / Decode<M>_FromObj are internal helpers (for same-package/cross-package codegen to nest calls into each other);
 *   business code should use the main Encode/Decode/Free.
 */
#ifndef MOP_CHANNEL_JSON_H
#define MOP_CHANNEL_JSON_H

#include "mop_channel_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode MopChannelJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode MopChannelJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *MopChannelJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *MopChannelJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *MopChannelJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           MopChannelJson_EncodeMopEnableRequest(const T_MopEnableRequest *src);
T_ThingModelReturnCode  MopChannelJson_DecodeMopEnableRequest(const char *json, T_MopEnableRequest *out);
void             MopChannel_FreeMopEnableRequest(T_MopEnableRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *MopChannelJson_EncodeMopEnableRequest_AsObj(const T_MopEnableRequest *src);
T_ThingModelReturnCode MopChannelJson_DecodeMopEnableRequest_FromObj(const ThingModelJsonObj_t *obj, T_MopEnableRequest *out);
char *           MopChannelJson_EncodeMopEnableReply(const T_MopEnableReply *src);
T_ThingModelReturnCode  MopChannelJson_DecodeMopEnableReply(const char *json, T_MopEnableReply *out);
void             MopChannel_FreeMopEnableReply(T_MopEnableReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *MopChannelJson_EncodeMopEnableReply_AsObj(const T_MopEnableReply *src);
T_ThingModelReturnCode MopChannelJson_DecodeMopEnableReply_FromObj(const ThingModelJsonObj_t *obj, T_MopEnableReply *out);
char *           MopChannelJson_EncodeMopDisableRequest(const T_MopDisableRequest *src);
T_ThingModelReturnCode  MopChannelJson_DecodeMopDisableRequest(const char *json, T_MopDisableRequest *out);
void             MopChannel_FreeMopDisableRequest(T_MopDisableRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *MopChannelJson_EncodeMopDisableRequest_AsObj(const T_MopDisableRequest *src);
T_ThingModelReturnCode MopChannelJson_DecodeMopDisableRequest_FromObj(const ThingModelJsonObj_t *obj, T_MopDisableRequest *out);
char *           MopChannelJson_EncodeChannelEventPush(const T_ChannelEventPush *src);
T_ThingModelReturnCode  MopChannelJson_DecodeChannelEventPush(const char *json, T_ChannelEventPush *out);
void             MopChannel_FreeChannelEventPush(T_ChannelEventPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *MopChannelJson_EncodeChannelEventPush_AsObj(const T_ChannelEventPush *src);
T_ThingModelReturnCode MopChannelJson_DecodeChannelEventPush_FromObj(const ThingModelJsonObj_t *obj, T_ChannelEventPush *out);

#ifdef __cplusplus
}
#endif
#endif /* MOP_CHANNEL_JSON_H */

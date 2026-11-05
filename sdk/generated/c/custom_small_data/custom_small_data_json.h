/**
 * @file    custom_small_data_json.h
 * @brief   CustomSmallData struct ↔ cJSON conversion.
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
#ifndef CUSTOM_SMALL_DATA_JSON_H
#define CUSTOM_SMALL_DATA_JSON_H

#include "custom_small_data_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode CustomSmallDataJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode CustomSmallDataJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *CustomSmallDataJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *CustomSmallDataJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *CustomSmallDataJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           CustomSmallDataJson_EncodeTransmitRequest(const T_TransmitRequest *src);
T_ThingModelReturnCode  CustomSmallDataJson_DecodeTransmitRequest(const char *json, T_TransmitRequest *out);
void             CustomSmallData_FreeTransmitRequest(T_TransmitRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *CustomSmallDataJson_EncodeTransmitRequest_AsObj(const T_TransmitRequest *src);
T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitRequest_FromObj(const ThingModelJsonObj_t *obj, T_TransmitRequest *out);
char *           CustomSmallDataJson_EncodeTransmitEventPush(const T_TransmitEventPush *src);
T_ThingModelReturnCode  CustomSmallDataJson_DecodeTransmitEventPush(const char *json, T_TransmitEventPush *out);
void             CustomSmallData_FreeTransmitEventPush(T_TransmitEventPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *CustomSmallDataJson_EncodeTransmitEventPush_AsObj(const T_TransmitEventPush *src);
T_ThingModelReturnCode CustomSmallDataJson_DecodeTransmitEventPush_FromObj(const ThingModelJsonObj_t *obj, T_TransmitEventPush *out);

#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_JSON_H */

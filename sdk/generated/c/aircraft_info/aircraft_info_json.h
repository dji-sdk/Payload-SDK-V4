/**
 * @file    aircraft_info_json.h
 * @brief   AircraftInfo struct ↔ cJSON conversion.
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
#ifndef AIRCRAFT_INFO_JSON_H
#define AIRCRAFT_INFO_JSON_H

#include "aircraft_info_types.h"
#include "thing_model_client/thing_model_client_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode AircraftInfoJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode AircraftInfoJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *AircraftInfoJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *AircraftInfoJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *AircraftInfoJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           AircraftInfoJson_EncodeMobileAppInfo(const T_MobileAppInfo *src);
T_ThingModelReturnCode  AircraftInfoJson_DecodeMobileAppInfo(const char *json, T_MobileAppInfo *out);
void             AircraftInfo_FreeMobileAppInfo(T_MobileAppInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *AircraftInfoJson_EncodeMobileAppInfo_AsObj(const T_MobileAppInfo *src);
T_ThingModelReturnCode AircraftInfoJson_DecodeMobileAppInfo_FromObj(const ThingModelJsonObj_t *obj, T_MobileAppInfo *out);
char *           AircraftInfoJson_EncodeAircraftDeviceInfo(const T_AircraftDeviceInfo *src);
T_ThingModelReturnCode  AircraftInfoJson_DecodeAircraftDeviceInfo(const char *json, T_AircraftDeviceInfo *out);
void             AircraftInfo_FreeAircraftDeviceInfo(T_AircraftDeviceInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *AircraftInfoJson_EncodeAircraftDeviceInfo_AsObj(const T_AircraftDeviceInfo *src);
T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftDeviceInfo_FromObj(const ThingModelJsonObj_t *obj, T_AircraftDeviceInfo *out);
char *           AircraftInfoJson_EncodeAircraftInfoPush(const T_AircraftInfoPush *src);
T_ThingModelReturnCode  AircraftInfoJson_DecodeAircraftInfoPush(const char *json, T_AircraftInfoPush *out);
void             AircraftInfo_FreeAircraftInfoPush(T_AircraftInfoPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *AircraftInfoJson_EncodeAircraftInfoPush_AsObj(const T_AircraftInfoPush *src);
T_ThingModelReturnCode AircraftInfoJson_DecodeAircraftInfoPush_FromObj(const ThingModelJsonObj_t *obj, T_AircraftInfoPush *out);

#ifdef __cplusplus
}
#endif
#endif /* AIRCRAFT_INFO_JSON_H */

/**
 * @file    device_info_json.h
 * @brief   DeviceInfo struct ↔ cJSON conversion.
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
#ifndef DEVICE_INFO_JSON_H
#define DEVICE_INFO_JSON_H

#include "device_info_types.h"
#include "thing_model_client/thing_model_client_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode DeviceInfoJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode DeviceInfoJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *DeviceInfoJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *DeviceInfoJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *DeviceInfoJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           DeviceInfoJson_EncodeDeviceInfoPush(const T_DeviceInfoPush *src);
T_ThingModelReturnCode  DeviceInfoJson_DecodeDeviceInfoPush(const char *json, T_DeviceInfoPush *out);
void             DeviceInfo_FreeDeviceInfoPush(T_DeviceInfoPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DeviceInfoJson_EncodeDeviceInfoPush_AsObj(const T_DeviceInfoPush *src);
T_ThingModelReturnCode DeviceInfoJson_DecodeDeviceInfoPush_FromObj(const ThingModelJsonObj_t *obj, T_DeviceInfoPush *out);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_INFO_JSON_H */

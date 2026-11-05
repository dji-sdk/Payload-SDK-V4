/**
 * @file    power_json.h
 * @brief   Power struct ↔ cJSON conversion.
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
#ifndef POWER_JSON_H
#define POWER_JSON_H

#include "power_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode PowerJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode PowerJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *PowerJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *PowerJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *PowerJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           PowerJson_EncodePowerOffNotificationReply(const T_PowerOffNotificationReply *src);
T_ThingModelReturnCode  PowerJson_DecodePowerOffNotificationReply(const char *json, T_PowerOffNotificationReply *out);
void             Power_FreePowerOffNotificationReply(T_PowerOffNotificationReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *PowerJson_EncodePowerOffNotificationReply_AsObj(const T_PowerOffNotificationReply *src);
T_ThingModelReturnCode PowerJson_DecodePowerOffNotificationReply_FromObj(const ThingModelJsonObj_t *obj, T_PowerOffNotificationReply *out);

#ifdef __cplusplus
}
#endif
#endif /* POWER_JSON_H */

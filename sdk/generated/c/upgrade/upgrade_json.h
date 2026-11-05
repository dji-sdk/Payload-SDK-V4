/**
 * @file    upgrade_json.h
 * @brief   Upgrade struct ↔ cJSON conversion.
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
#ifndef UPGRADE_JSON_H
#define UPGRADE_JSON_H

#include "upgrade_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode UpgradeJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode UpgradeJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *UpgradeJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *UpgradeJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *UpgradeJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           UpgradeJson_EncodeUpgradeEnterModeReply(const T_UpgradeEnterModeReply *src);
T_ThingModelReturnCode  UpgradeJson_DecodeUpgradeEnterModeReply(const char *json, T_UpgradeEnterModeReply *out);
void             Upgrade_FreeUpgradeEnterModeReply(T_UpgradeEnterModeReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *UpgradeJson_EncodeUpgradeEnterModeReply_AsObj(const T_UpgradeEnterModeReply *src);
T_ThingModelReturnCode UpgradeJson_DecodeUpgradeEnterModeReply_FromObj(const ThingModelJsonObj_t *obj, T_UpgradeEnterModeReply *out);
char *           UpgradeJson_EncodeUpgradeStatePush(const T_UpgradeStatePush *src);
T_ThingModelReturnCode  UpgradeJson_DecodeUpgradeStatePush(const char *json, T_UpgradeStatePush *out);
void             Upgrade_FreeUpgradeStatePush(T_UpgradeStatePush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *UpgradeJson_EncodeUpgradeStatePush_AsObj(const T_UpgradeStatePush *src);
T_ThingModelReturnCode UpgradeJson_DecodeUpgradeStatePush_FromObj(const ThingModelJsonObj_t *obj, T_UpgradeStatePush *out);

#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_JSON_H */

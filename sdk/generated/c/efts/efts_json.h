/**
 * @file    efts_json.h
 * @brief   Efts struct ↔ cJSON conversion.
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
#ifndef EFTS_JSON_H
#define EFTS_JSON_H

#include "efts_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode EftsJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode EftsJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *EftsJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *EftsJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *EftsJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           EftsJson_EncodeEftsGetFtsTriggerStatusReply(const T_EftsGetFtsTriggerStatusReply *src);
T_ThingModelReturnCode  EftsJson_DecodeEftsGetFtsTriggerStatusReply(const char *json, T_EftsGetFtsTriggerStatusReply *out);
void             Efts_FreeEftsGetFtsTriggerStatusReply(T_EftsGetFtsTriggerStatusReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *EftsJson_EncodeEftsGetFtsTriggerStatusReply_AsObj(const T_EftsGetFtsTriggerStatusReply *src);
T_ThingModelReturnCode EftsJson_DecodeEftsGetFtsTriggerStatusReply_FromObj(const ThingModelJsonObj_t *obj, T_EftsGetFtsTriggerStatusReply *out);

#ifdef __cplusplus
}
#endif
#endif /* EFTS_JSON_H */

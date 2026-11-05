/**
 * @file    time_sync_json.h
 * @brief   TimeSync struct ↔ cJSON conversion.
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
#ifndef TIME_SYNC_JSON_H
#define TIME_SYNC_JSON_H

#include "time_sync_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode TimeSyncJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode TimeSyncJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *TimeSyncJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *TimeSyncJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *TimeSyncJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           TimeSyncJson_EncodeTimeSyncSetSignalModeRequest(const T_TimeSyncSetSignalModeRequest *src);
T_ThingModelReturnCode  TimeSyncJson_DecodeTimeSyncSetSignalModeRequest(const char *json, T_TimeSyncSetSignalModeRequest *out);
void             TimeSync_FreeTimeSyncSetSignalModeRequest(T_TimeSyncSetSignalModeRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *TimeSyncJson_EncodeTimeSyncSetSignalModeRequest_AsObj(const T_TimeSyncSetSignalModeRequest *src);
T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncSetSignalModeRequest_FromObj(const ThingModelJsonObj_t *obj, T_TimeSyncSetSignalModeRequest *out);
char *           TimeSyncJson_EncodeTimeSyncTimeSyncPush(const T_TimeSyncTimeSyncPush *src);
T_ThingModelReturnCode  TimeSyncJson_DecodeTimeSyncTimeSyncPush(const char *json, T_TimeSyncTimeSyncPush *out);
void             TimeSync_FreeTimeSyncTimeSyncPush(T_TimeSyncTimeSyncPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *TimeSyncJson_EncodeTimeSyncTimeSyncPush_AsObj(const T_TimeSyncTimeSyncPush *src);
T_ThingModelReturnCode TimeSyncJson_DecodeTimeSyncTimeSyncPush_FromObj(const ThingModelJsonObj_t *obj, T_TimeSyncTimeSyncPush *out);

#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_JSON_H */

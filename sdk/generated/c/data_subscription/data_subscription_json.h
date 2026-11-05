/**
 * @file    data_subscription_json.h
 * @brief   DataSubscription struct ↔ cJSON conversion.
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
#ifndef DATA_SUBSCRIPTION_JSON_H
#define DATA_SUBSCRIPTION_JSON_H

#include "data_subscription_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode DataSubscriptionJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode DataSubscriptionJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *DataSubscriptionJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *DataSubscriptionJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *DataSubscriptionJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           DataSubscriptionJson_EncodeDataSubscriptionSubscribeTopicRequest(const T_DataSubscriptionSubscribeTopicRequest *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionSubscribeTopicRequest(const char *json, T_DataSubscriptionSubscribeTopicRequest *out);
void             DataSubscription_FreeDataSubscriptionSubscribeTopicRequest(T_DataSubscriptionSubscribeTopicRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionSubscribeTopicRequest_AsObj(const T_DataSubscriptionSubscribeTopicRequest *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionSubscribeTopicRequest_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionSubscribeTopicRequest *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionUnsubscribeTopicRequest(const T_DataSubscriptionUnsubscribeTopicRequest *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionUnsubscribeTopicRequest(const char *json, T_DataSubscriptionUnsubscribeTopicRequest *out);
void             DataSubscription_FreeDataSubscriptionUnsubscribeTopicRequest(T_DataSubscriptionUnsubscribeTopicRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionUnsubscribeTopicRequest_AsObj(const T_DataSubscriptionUnsubscribeTopicRequest *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionUnsubscribeTopicRequest_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionUnsubscribeTopicRequest *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGetLatestValueRequest(const T_DataSubscriptionGetLatestValueRequest *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGetLatestValueRequest(const char *json, T_DataSubscriptionGetLatestValueRequest *out);
void             DataSubscription_FreeDataSubscriptionGetLatestValueRequest(T_DataSubscriptionGetLatestValueRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGetLatestValueRequest_AsObj(const T_DataSubscriptionGetLatestValueRequest *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGetLatestValueRequest_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGetLatestValueRequest *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionQuaternion(const T_DataSubscriptionQuaternion *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionQuaternion(const char *json, T_DataSubscriptionQuaternion *out);
void             DataSubscription_FreeDataSubscriptionQuaternion(T_DataSubscriptionQuaternion *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionQuaternion_AsObj(const T_DataSubscriptionQuaternion *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionQuaternion_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionQuaternion *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionVector3f(const T_DataSubscriptionVector3f *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionVector3f(const char *json, T_DataSubscriptionVector3f *out);
void             DataSubscription_FreeDataSubscriptionVector3f(T_DataSubscriptionVector3f *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionVector3f_AsObj(const T_DataSubscriptionVector3f *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3f_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionVector3f *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionVelocity(const T_DataSubscriptionVelocity *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionVelocity(const char *json, T_DataSubscriptionVelocity *out);
void             DataSubscription_FreeDataSubscriptionVelocity(T_DataSubscriptionVelocity *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionVelocity_AsObj(const T_DataSubscriptionVelocity *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVelocity_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionVelocity *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionAltitude(const T_DataSubscriptionAltitude *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionAltitude(const char *json, T_DataSubscriptionAltitude *out);
void             DataSubscription_FreeDataSubscriptionAltitude(T_DataSubscriptionAltitude *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionAltitude_AsObj(const T_DataSubscriptionAltitude *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAltitude_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionAltitude *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionHeight(const T_DataSubscriptionHeight *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionHeight(const char *json, T_DataSubscriptionHeight *out);
void             DataSubscription_FreeDataSubscriptionHeight(T_DataSubscriptionHeight *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionHeight_AsObj(const T_DataSubscriptionHeight *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHeight_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionHeight *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionPositionFused(const T_DataSubscriptionPositionFused *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionPositionFused(const char *json, T_DataSubscriptionPositionFused *out);
void             DataSubscription_FreeDataSubscriptionPositionFused(T_DataSubscriptionPositionFused *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionPositionFused_AsObj(const T_DataSubscriptionPositionFused *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionFused_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionPositionFused *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionPositionVo(const T_DataSubscriptionPositionVo *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionPositionVo(const char *json, T_DataSubscriptionPositionVo *out);
void             DataSubscription_FreeDataSubscriptionPositionVo(T_DataSubscriptionPositionVo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionPositionVo_AsObj(const T_DataSubscriptionPositionVo *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPositionVo_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionPositionVo *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGpsDate(const T_DataSubscriptionGpsDate *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGpsDate(const char *json, T_DataSubscriptionGpsDate *out);
void             DataSubscription_FreeDataSubscriptionGpsDate(T_DataSubscriptionGpsDate *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGpsDate_AsObj(const T_DataSubscriptionGpsDate *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDate_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGpsDate *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGpsTime(const T_DataSubscriptionGpsTime *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGpsTime(const char *json, T_DataSubscriptionGpsTime *out);
void             DataSubscription_FreeDataSubscriptionGpsTime(T_DataSubscriptionGpsTime *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGpsTime_AsObj(const T_DataSubscriptionGpsTime *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsTime_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGpsTime *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionVector3i(const T_DataSubscriptionVector3i *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionVector3i(const char *json, T_DataSubscriptionVector3i *out);
void             DataSubscription_FreeDataSubscriptionVector3i(T_DataSubscriptionVector3i *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionVector3i_AsObj(const T_DataSubscriptionVector3i *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionVector3i_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionVector3i *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGpsDetails(const T_DataSubscriptionGpsDetails *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGpsDetails(const char *json, T_DataSubscriptionGpsDetails *out);
void             DataSubscription_FreeDataSubscriptionGpsDetails(T_DataSubscriptionGpsDetails *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGpsDetails_AsObj(const T_DataSubscriptionGpsDetails *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsDetails_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGpsDetails *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel(const T_DataSubscriptionGpsSignalLevel *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel(const char *json, T_DataSubscriptionGpsSignalLevel *out);
void             DataSubscription_FreeDataSubscriptionGpsSignalLevel(T_DataSubscriptionGpsSignalLevel *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGpsSignalLevel_AsObj(const T_DataSubscriptionGpsSignalLevel *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGpsSignalLevel_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGpsSignalLevel *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRtkYaw(const T_DataSubscriptionRtkYaw *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRtkYaw(const char *json, T_DataSubscriptionRtkYaw *out);
void             DataSubscription_FreeDataSubscriptionRtkYaw(T_DataSubscriptionRtkYaw *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRtkYaw_AsObj(const T_DataSubscriptionRtkYaw *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYaw_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRtkYaw *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo(const T_DataSubscriptionRtkPositionInfo *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo(const char *json, T_DataSubscriptionRtkPositionInfo *out);
void             DataSubscription_FreeDataSubscriptionRtkPositionInfo(T_DataSubscriptionRtkPositionInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRtkPositionInfo_AsObj(const T_DataSubscriptionRtkPositionInfo *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkPositionInfo_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRtkPositionInfo *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo(const T_DataSubscriptionRtkYawInfo *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo(const char *json, T_DataSubscriptionRtkYawInfo *out);
void             DataSubscription_FreeDataSubscriptionRtkYawInfo(T_DataSubscriptionRtkYawInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRtkYawInfo_AsObj(const T_DataSubscriptionRtkYawInfo *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkYawInfo_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRtkYawInfo *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus(const T_DataSubscriptionRtkConnectStatus *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus(const char *json, T_DataSubscriptionRtkConnectStatus *out);
void             DataSubscription_FreeDataSubscriptionRtkConnectStatus(T_DataSubscriptionRtkConnectStatus *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRtkConnectStatus_AsObj(const T_DataSubscriptionRtkConnectStatus *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRtkConnectStatus_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRtkConnectStatus *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRc(const T_DataSubscriptionRc *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRc(const char *json, T_DataSubscriptionRc *out);
void             DataSubscription_FreeDataSubscriptionRc(T_DataSubscriptionRc *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRc_AsObj(const T_DataSubscriptionRc *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRc_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRc *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData(const T_DataSubscriptionRcWithFlagData *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData(const char *json, T_DataSubscriptionRcWithFlagData *out);
void             DataSubscription_FreeDataSubscriptionRcWithFlagData(T_DataSubscriptionRcWithFlagData *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionRcWithFlagData_AsObj(const T_DataSubscriptionRcWithFlagData *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionRcWithFlagData_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionRcWithFlagData *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionStatusFlight(const T_DataSubscriptionStatusFlight *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionStatusFlight(const char *json, T_DataSubscriptionStatusFlight *out);
void             DataSubscription_FreeDataSubscriptionStatusFlight(T_DataSubscriptionStatusFlight *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionStatusFlight_AsObj(const T_DataSubscriptionStatusFlight *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusFlight_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionStatusFlight *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode(const T_DataSubscriptionStatusDisplayMode *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode(const char *json, T_DataSubscriptionStatusDisplayMode *out);
void             DataSubscription_FreeDataSubscriptionStatusDisplayMode(T_DataSubscriptionStatusDisplayMode *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionStatusDisplayMode_AsObj(const T_DataSubscriptionStatusDisplayMode *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionStatusDisplayMode_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionStatusDisplayMode *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionControlDevice(const T_DataSubscriptionControlDevice *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionControlDevice(const char *json, T_DataSubscriptionControlDevice *out);
void             DataSubscription_FreeDataSubscriptionControlDevice(T_DataSubscriptionControlDevice *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionControlDevice_AsObj(const T_DataSubscriptionControlDevice *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionControlDevice_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionControlDevice *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionEscItem(const T_DataSubscriptionEscItem *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionEscItem(const char *json, T_DataSubscriptionEscItem *out);
void             DataSubscription_FreeDataSubscriptionEscItem(T_DataSubscriptionEscItem *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionEscItem_AsObj(const T_DataSubscriptionEscItem *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionEscItem_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionEscItem *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly(const T_DataSubscriptionFlightAnomaly *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly(const char *json, T_DataSubscriptionFlightAnomaly *out);
void             DataSubscription_FreeDataSubscriptionFlightAnomaly(T_DataSubscriptionFlightAnomaly *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionFlightAnomaly_AsObj(const T_DataSubscriptionFlightAnomaly *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionFlightAnomaly_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionFlightAnomaly *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionAvoidData(const T_DataSubscriptionAvoidData *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionAvoidData(const char *json, T_DataSubscriptionAvoidData *out);
void             DataSubscription_FreeDataSubscriptionAvoidData(T_DataSubscriptionAvoidData *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionAvoidData_AsObj(const T_DataSubscriptionAvoidData *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionAvoidData_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionAvoidData *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus(const T_DataSubscriptionHomePointSetStatus *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus(const char *json, T_DataSubscriptionHomePointSetStatus *out);
void             DataSubscription_FreeDataSubscriptionHomePointSetStatus(T_DataSubscriptionHomePointSetStatus *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionHomePointSetStatus_AsObj(const T_DataSubscriptionHomePointSetStatus *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointSetStatus_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionHomePointSetStatus *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo(const T_DataSubscriptionHomePointInfo *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo(const char *json, T_DataSubscriptionHomePointInfo *out);
void             DataSubscription_FreeDataSubscriptionHomePointInfo(T_DataSubscriptionHomePointInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionHomePointInfo_AsObj(const T_DataSubscriptionHomePointInfo *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionHomePointInfo_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionHomePointInfo *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionBatteryState(const T_DataSubscriptionBatteryState *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionBatteryState(const char *json, T_DataSubscriptionBatteryState *out);
void             DataSubscription_FreeDataSubscriptionBatteryState(T_DataSubscriptionBatteryState *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionBatteryState_AsObj(const T_DataSubscriptionBatteryState *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatteryState_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionBatteryState *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo(const T_DataSubscriptionBatterySingleInfo *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo(const char *json, T_DataSubscriptionBatterySingleInfo *out);
void             DataSubscription_FreeDataSubscriptionBatterySingleInfo(T_DataSubscriptionBatterySingleInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionBatterySingleInfo_AsObj(const T_DataSubscriptionBatterySingleInfo *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionBatterySingleInfo_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionBatterySingleInfo *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles(const T_DataSubscriptionGimbalAngles *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles(const char *json, T_DataSubscriptionGimbalAngles *out);
void             DataSubscription_FreeDataSubscriptionGimbalAngles(T_DataSubscriptionGimbalAngles *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionGimbalAngles_AsObj(const T_DataSubscriptionGimbalAngles *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionGimbalAngles_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionGimbalAngles *out);
char *           DataSubscriptionJson_EncodeDataSubscriptionPublishDataPush(const T_DataSubscriptionPublishDataPush *src);
T_ThingModelReturnCode  DataSubscriptionJson_DecodeDataSubscriptionPublishDataPush(const char *json, T_DataSubscriptionPublishDataPush *out);
void             DataSubscription_FreeDataSubscriptionPublishDataPush(T_DataSubscriptionPublishDataPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *DataSubscriptionJson_EncodeDataSubscriptionPublishDataPush_AsObj(const T_DataSubscriptionPublishDataPush *src);
T_ThingModelReturnCode DataSubscriptionJson_DecodeDataSubscriptionPublishDataPush_FromObj(const ThingModelJsonObj_t *obj, T_DataSubscriptionPublishDataPush *out);

#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_JSON_H */

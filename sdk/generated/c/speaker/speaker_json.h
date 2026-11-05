/**
 * @file    speaker_json.h
 * @brief   Speaker struct ↔ cJSON conversion.
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
#ifndef SPEAKER_JSON_H
#define SPEAKER_JSON_H

#include "speaker_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode SpeakerJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode SpeakerJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *SpeakerJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *SpeakerJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *SpeakerJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           SpeakerJson_EncodeSpeakerSetWorkModeRequest(const T_SpeakerSetWorkModeRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerSetWorkModeRequest(const char *json, T_SpeakerSetWorkModeRequest *out);
void             Speaker_FreeSpeakerSetWorkModeRequest(T_SpeakerSetWorkModeRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerSetWorkModeRequest_AsObj(const T_SpeakerSetWorkModeRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetWorkModeRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerSetWorkModeRequest *out);
char *           SpeakerJson_EncodeSpeakerSetPlayModeRequest(const T_SpeakerSetPlayModeRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerSetPlayModeRequest(const char *json, T_SpeakerSetPlayModeRequest *out);
void             Speaker_FreeSpeakerSetPlayModeRequest(T_SpeakerSetPlayModeRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerSetPlayModeRequest_AsObj(const T_SpeakerSetPlayModeRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetPlayModeRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerSetPlayModeRequest *out);
char *           SpeakerJson_EncodeSpeakerSetVolumeRequest(const T_SpeakerSetVolumeRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerSetVolumeRequest(const char *json, T_SpeakerSetVolumeRequest *out);
void             Speaker_FreeSpeakerSetVolumeRequest(T_SpeakerSetVolumeRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerSetVolumeRequest_AsObj(const T_SpeakerSetVolumeRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerSetVolumeRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerSetVolumeRequest *out);
char *           SpeakerJson_EncodeSpeakerTransStartRequest(const T_SpeakerTransStartRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerTransStartRequest(const char *json, T_SpeakerTransStartRequest *out);
void             Speaker_FreeSpeakerTransStartRequest(T_SpeakerTransStartRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerTransStartRequest_AsObj(const T_SpeakerTransStartRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransStartRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerTransStartRequest *out);
char *           SpeakerJson_EncodeSpeakerTransDataRequest(const T_SpeakerTransDataRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerTransDataRequest(const char *json, T_SpeakerTransDataRequest *out);
void             Speaker_FreeSpeakerTransDataRequest(T_SpeakerTransDataRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerTransDataRequest_AsObj(const T_SpeakerTransDataRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransDataRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerTransDataRequest *out);
char *           SpeakerJson_EncodeSpeakerTransFinishRequest(const T_SpeakerTransFinishRequest *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerTransFinishRequest(const char *json, T_SpeakerTransFinishRequest *out);
void             Speaker_FreeSpeakerTransFinishRequest(T_SpeakerTransFinishRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerTransFinishRequest_AsObj(const T_SpeakerTransFinishRequest *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerTransFinishRequest_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerTransFinishRequest *out);
char *           SpeakerJson_EncodeSpeakerStatePush(const T_SpeakerStatePush *src);
T_ThingModelReturnCode  SpeakerJson_DecodeSpeakerStatePush(const char *json, T_SpeakerStatePush *out);
void             Speaker_FreeSpeakerStatePush(T_SpeakerStatePush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *SpeakerJson_EncodeSpeakerStatePush_AsObj(const T_SpeakerStatePush *src);
T_ThingModelReturnCode SpeakerJson_DecodeSpeakerStatePush_FromObj(const ThingModelJsonObj_t *obj, T_SpeakerStatePush *out);

#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_JSON_H */

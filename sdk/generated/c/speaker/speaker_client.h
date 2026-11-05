/**
 * @file    speaker_client.h
 * @brief   Speaker client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef SPEAKER_CLIENT_H
#define SPEAKER_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "speaker_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Speaker_Client Speaker_Client_t;

Speaker_Client_t *Speaker_CreateClient (ThingModelClientSession_t *session);
void              Speaker_DestroyClient(Speaker_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *Speaker_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *Speaker_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */

T_ThingModelReturnCode Speaker_SetWorkMode(Speaker_Client_t *c, const T_SpeakerSetWorkModeRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_SetWorkModeAsync(Speaker_Client_t *c, const T_SpeakerSetWorkModeRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildSetWorkModePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerSetWorkModeRequest *req);
T_ThingModelReturnCode Speaker_SetPlayMode(Speaker_Client_t *c, const T_SpeakerSetPlayModeRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_SetPlayModeAsync(Speaker_Client_t *c, const T_SpeakerSetPlayModeRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildSetPlayModePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerSetPlayModeRequest *req);
T_ThingModelReturnCode Speaker_SetVolume(Speaker_Client_t *c, const T_SpeakerSetVolumeRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_SetVolumeAsync(Speaker_Client_t *c, const T_SpeakerSetVolumeRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildSetVolumePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerSetVolumeRequest *req);
T_ThingModelReturnCode Speaker_StartPlay(Speaker_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_StartPlayAsync(Speaker_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildStartPlayPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Speaker_StopPlay(Speaker_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_StopPlayAsync(Speaker_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildStopPlayPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Speaker_TransStart(Speaker_Client_t *c, const T_SpeakerTransStartRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_TransStartAsync(Speaker_Client_t *c, const T_SpeakerTransStartRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildTransStartPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerTransStartRequest *req);
T_ThingModelReturnCode Speaker_TransData(Speaker_Client_t *c, const T_SpeakerTransDataRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_TransDataAsync(Speaker_Client_t *c, const T_SpeakerTransDataRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildTransDataPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerTransDataRequest *req);
T_ThingModelReturnCode Speaker_TransFinish(Speaker_Client_t *c, const T_SpeakerTransFinishRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Speaker_TransFinishAsync(Speaker_Client_t *c, const T_SpeakerTransFinishRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Speaker_BuildTransFinishPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_SpeakerTransFinishRequest *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*Speaker_State_Cb)(void *user, const T_SpeakerStatePush *push);
T_ThingModelReturnCode Speaker_OnState(Speaker_Client_t *c, Speaker_State_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_CLIENT_H */

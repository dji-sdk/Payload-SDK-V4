/**
 * @file    speaker_impl.h
 * @brief   Speaker server-side implementation (impl) API: the device side registers handlers to receive cloud→device calls.
 *
 * Symmetric with the client side: the client calls, impl implements and returns a reply.
 * Covers the three categories SERVICE + PROPERTIES + PROPERTIES_READONLY (EVENT/OSD/STATE are one-way device→cloud pushes,
 * which need no impl-side handler — the device side just uses the Publisher API to send them directly).
 * Handlers run on the transport thread: do not call this cap's or the client's sync API inside them, and do not Destroy.
 */
#ifndef SPEAKER_IMPL_H
#define SPEAKER_IMPL_H

#include "thing_model_client/thing_model_client_session.h"
#include "speaker_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpeakerImpl SpeakerImpl_t;

/**
 * Create impl: subscribes to the services / property/get / property/set topics and is ready to receive cloud→device calls.
 * @param session   an already-Connected session
 * @param device_id which device's topic to listen on (usually = cfg.device_sn, may differ — used for proxy/testing)
 */
SpeakerImpl_t *SpeakerImpl_Create (ThingModelClientSession_t *session, const char *device_id);
void               SpeakerImpl_Destroy(SpeakerImpl_t *impl);

/** Protocol code version this SDK was built against. */
const char *SpeakerImpl_GeneratorVersion(void);

/* ============================================================ */
/* SERVICE handler registration                                  */
/* ============================================================ */
/*
 * One handler typedef + On<Rpc> registration API per SERVICE rpc.
 *   - 1st arg impl  = this impl instance
 *   - 2nd arg user  = the user pointer passed at registration
 *   - 3rd arg req   = the decoded request (read-only)
 *   - 4th arg reply = the reply struct you fill in (only sent back when returning RC_OK)
 *   - return THING_MODEL_RC_OK → runtime encodes *reply and publishes it to services_reply
 *   - return non-OK           → runtime sends a standard error reply (result=rc)
 * Multiple registrations for the same rpc: a later registration overrides an earlier one; pass h=NULL to unregister.
 */
/** SetWorkMode handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_SetWorkMode_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerSetWorkModeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnSetWorkMode(SpeakerImpl_t *impl, SpeakerImpl_SetWorkMode_Handler h, void *user);
/** SetPlayMode handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_SetPlayMode_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerSetPlayModeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnSetPlayMode(SpeakerImpl_t *impl, SpeakerImpl_SetPlayMode_Handler h, void *user);
/** SetVolume handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_SetVolume_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerSetVolumeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnSetVolume(SpeakerImpl_t *impl, SpeakerImpl_SetVolume_Handler h, void *user);
/** StartPlay handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_StartPlay_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnStartPlay(SpeakerImpl_t *impl, SpeakerImpl_StartPlay_Handler h, void *user);
/** StopPlay handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_StopPlay_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnStopPlay(SpeakerImpl_t *impl, SpeakerImpl_StopPlay_Handler h, void *user);
/** TransStart handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_TransStart_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerTransStartRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnTransStart(SpeakerImpl_t *impl, SpeakerImpl_TransStart_Handler h, void *user);
/** TransData handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_TransData_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerTransDataRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnTransData(SpeakerImpl_t *impl, SpeakerImpl_TransData_Handler h, void *user);
/** TransFinish handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*SpeakerImpl_TransFinish_Handler)(
    SpeakerImpl_t *impl, void *user,
    const T_SpeakerTransFinishRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode SpeakerImpl_OnTransFinish(SpeakerImpl_t *impl, SpeakerImpl_TransFinish_Handler h, void *user);


#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_IMPL_H */

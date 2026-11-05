/**
 * @file    time_sync_impl.h
 * @brief   TimeSync server-side implementation (impl) API: the device side registers handlers to receive cloud→device calls.
 *
 * Symmetric with the client side: the client calls, impl implements and returns a reply.
 * Covers the three categories SERVICE + PROPERTIES + PROPERTIES_READONLY (EVENT/OSD/STATE are one-way device→cloud pushes,
 * which need no impl-side handler — the device side just uses the Publisher API to send them directly).
 * Handlers run on the transport thread: do not call this cap's or the client's sync API inside them, and do not Destroy.
 */
#ifndef TIME_SYNC_IMPL_H
#define TIME_SYNC_IMPL_H

#include "thing_model_client/thing_model_client_session.h"
#include "time_sync_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TimeSyncImpl TimeSyncImpl_t;

/**
 * Create impl: subscribes to the services / property/get / property/set topics and is ready to receive cloud→device calls.
 * @param session   an already-Connected session
 * @param device_id which device's topic to listen on (usually = cfg.device_sn, may differ — used for proxy/testing)
 */
TimeSyncImpl_t *TimeSyncImpl_Create (ThingModelClientSession_t *session, const char *device_id);
void               TimeSyncImpl_Destroy(TimeSyncImpl_t *impl);

/** Protocol code version this SDK was built against. */
const char *TimeSyncImpl_GeneratorVersion(void);

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
/** SetSignalMode handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*TimeSyncImpl_SetSignalMode_Handler)(
    TimeSyncImpl_t *impl, void *user,
    const T_TimeSyncSetSignalModeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode TimeSyncImpl_OnSetSignalMode(TimeSyncImpl_t *impl, TimeSyncImpl_SetSignalMode_Handler h, void *user);


#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_IMPL_H */

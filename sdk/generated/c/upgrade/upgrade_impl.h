/**
 * @file    upgrade_impl.h
 * @brief   Upgrade server-side implementation (impl) API: the device side registers handlers to receive cloud→device calls.
 *
 * Symmetric with the client side: the client calls, impl implements and returns a reply.
 * Covers the three categories SERVICE + PROPERTIES + PROPERTIES_READONLY (EVENT/OSD/STATE are one-way device→cloud pushes,
 * which need no impl-side handler — the device side just uses the Publisher API to send them directly).
 * Handlers run on the transport thread: do not call this cap's or the client's sync API inside them, and do not Destroy.
 */
#ifndef UPGRADE_IMPL_H
#define UPGRADE_IMPL_H

#include "thing_model_client/thing_model_client_session.h"
#include "upgrade_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UpgradeImpl UpgradeImpl_t;

/**
 * Create impl: subscribes to the services / property/get / property/set topics and is ready to receive cloud→device calls.
 * @param session   an already-Connected session
 * @param device_id which device's topic to listen on (usually = cfg.device_sn, may differ — used for proxy/testing)
 */
UpgradeImpl_t *UpgradeImpl_Create (ThingModelClientSession_t *session, const char *device_id);
void               UpgradeImpl_Destroy(UpgradeImpl_t *impl);

/** Protocol code version this SDK was built against. */
const char *UpgradeImpl_GeneratorVersion(void);

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
/** EnterUpgradeMode handler — typed reply T_UpgradeEnterModeReply (proto: returns (UpgradeEnterModeReply)) */
typedef T_ThingModelReturnCode (*UpgradeImpl_EnterUpgradeMode_Handler)(
    UpgradeImpl_t *impl, void *user,
    const T_Placeholder *req, T_UpgradeEnterModeReply *reply);
T_ThingModelReturnCode UpgradeImpl_OnEnterUpgradeMode(UpgradeImpl_t *impl, UpgradeImpl_EnterUpgradeMode_Handler h, void *user);
/** CheckFirmware handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*UpgradeImpl_CheckFirmware_Handler)(
    UpgradeImpl_t *impl, void *user,
    const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode UpgradeImpl_OnCheckFirmware(UpgradeImpl_t *impl, UpgradeImpl_CheckFirmware_Handler h, void *user);
/** StartUpgrade handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*UpgradeImpl_StartUpgrade_Handler)(
    UpgradeImpl_t *impl, void *user,
    const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode UpgradeImpl_OnStartUpgrade(UpgradeImpl_t *impl, UpgradeImpl_StartUpgrade_Handler h, void *user);
/** FinishUpgrade handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*UpgradeImpl_FinishUpgrade_Handler)(
    UpgradeImpl_t *impl, void *user,
    const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode UpgradeImpl_OnFinishUpgrade(UpgradeImpl_t *impl, UpgradeImpl_FinishUpgrade_Handler h, void *user);


#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_IMPL_H */

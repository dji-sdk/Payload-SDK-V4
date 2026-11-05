/**
 * @file    upgrade_client.h
 * @brief   Upgrade client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef UPGRADE_CLIENT_H
#define UPGRADE_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "upgrade_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Upgrade_Client Upgrade_Client_t;

Upgrade_Client_t *Upgrade_CreateClient (ThingModelClientSession_t *session);
void              Upgrade_DestroyClient(Upgrade_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *Upgrade_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *Upgrade_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */
/** EnterUpgradeMode async callback — the decoded typed reply (proto declares returns (UpgradeEnterModeReply)) */
typedef void (*Upgrade_EnterUpgradeMode_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_UpgradeEnterModeReply *reply);

T_ThingModelReturnCode Upgrade_EnterUpgradeMode(Upgrade_Client_t *c, const T_Placeholder *req, T_UpgradeEnterModeReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Upgrade_EnterUpgradeModeAsync(Upgrade_Client_t *c, const T_Placeholder *req, Upgrade_EnterUpgradeMode_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Upgrade_BuildEnterUpgradeModePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Upgrade_CheckFirmware(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Upgrade_CheckFirmwareAsync(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Upgrade_BuildCheckFirmwarePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Upgrade_StartUpgrade(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Upgrade_StartUpgradeAsync(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Upgrade_BuildStartUpgradePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Upgrade_FinishUpgrade(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Upgrade_FinishUpgradeAsync(Upgrade_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Upgrade_BuildFinishUpgradePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*Upgrade_PushUpgradeState_Cb)(void *user, const T_UpgradeStatePush *push);
T_ThingModelReturnCode Upgrade_OnPushUpgradeState(Upgrade_Client_t *c, Upgrade_PushUpgradeState_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_CLIENT_H */

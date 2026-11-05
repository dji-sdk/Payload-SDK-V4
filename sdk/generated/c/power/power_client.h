/**
 * @file    power_client.h
 * @brief   Power client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef POWER_CLIENT_H
#define POWER_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "power_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Power_Client Power_Client_t;

Power_Client_t *Power_CreateClient (ThingModelClientSession_t *session);
void              Power_DestroyClient(Power_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *Power_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *Power_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */
/** PowerOffNotification async callback — the decoded typed reply (proto declares returns (PowerOffNotificationReply)) */
typedef void (*Power_PowerOffNotification_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_PowerOffNotificationReply *reply);

T_ThingModelReturnCode Power_ApplyHighPower(Power_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Power_ApplyHighPowerAsync(Power_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Power_BuildApplyHighPowerPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Power_PowerOffNotification(Power_Client_t *c, const T_Placeholder *req, T_PowerOffNotificationReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Power_PowerOffNotificationAsync(Power_Client_t *c, const T_Placeholder *req, Power_PowerOffNotification_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Power_BuildPowerOffNotificationPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);




#ifdef __cplusplus
}
#endif
#endif /* POWER_CLIENT_H */

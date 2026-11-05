/**
 * @file    efts_client.h
 * @brief   Efts client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef EFTS_CLIENT_H
#define EFTS_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "efts_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Efts_Client Efts_Client_t;

Efts_Client_t *Efts_CreateClient (ThingModelClientSession_t *session);
void              Efts_DestroyClient(Efts_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *Efts_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *Efts_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */
/** GetFtsTriggerStatus async callback — the decoded typed reply (proto declares returns (EftsGetFtsTriggerStatusReply)) */
typedef void (*Efts_GetFtsTriggerStatus_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_EftsGetFtsTriggerStatusReply *reply);

T_ThingModelReturnCode Efts_SelectFtsPwmTrigger(Efts_Client_t *c, const T_Placeholder *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Efts_SelectFtsPwmTriggerAsync(Efts_Client_t *c, const T_Placeholder *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Efts_BuildSelectFtsPwmTriggerPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);
T_ThingModelReturnCode Efts_GetFtsTriggerStatus(Efts_Client_t *c, const T_Placeholder *req, T_EftsGetFtsTriggerStatusReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Efts_GetFtsTriggerStatusAsync(Efts_Client_t *c, const T_Placeholder *req, Efts_GetFtsTriggerStatus_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Efts_BuildGetFtsTriggerStatusPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_Placeholder *req);




#ifdef __cplusplus
}
#endif
#endif /* EFTS_CLIENT_H */

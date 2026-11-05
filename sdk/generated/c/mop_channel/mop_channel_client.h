/**
 * @file    mop_channel_client.h
 * @brief   MopChannel client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef MOP_CHANNEL_CLIENT_H
#define MOP_CHANNEL_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "mop_channel_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MopChannel_Client MopChannel_Client_t;

MopChannel_Client_t *MopChannel_CreateClient (ThingModelClientSession_t *session);
void              MopChannel_DestroyClient(MopChannel_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *MopChannel_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *MopChannel_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */
/** Enable async callback — the decoded typed reply (proto declares returns (MopEnableReply)) */
typedef void (*MopChannel_Enable_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_MopEnableReply *reply);
/** Disable async callback — the decoded typed reply (proto declares returns (Placeholder)) */
typedef void (*MopChannel_Disable_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_Placeholder *reply);

T_ThingModelReturnCode MopChannel_Enable(MopChannel_Client_t *c, const T_MopEnableRequest *req, T_MopEnableReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode MopChannel_EnableAsync(MopChannel_Client_t *c, const T_MopEnableRequest *req, MopChannel_Enable_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *MopChannel_BuildEnablePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_MopEnableRequest *req);
T_ThingModelReturnCode MopChannel_Disable(MopChannel_Client_t *c, const T_MopDisableRequest *req, T_Placeholder *resp, uint32_t timeout_ms);
T_ThingModelReturnCode MopChannel_DisableAsync(MopChannel_Client_t *c, const T_MopDisableRequest *req, MopChannel_Disable_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *MopChannel_BuildDisablePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_MopDisableRequest *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*MopChannel_ChannelEvent_Cb)(void *user, const T_ChannelEventPush *push);
T_ThingModelReturnCode MopChannel_OnChannelEvent(MopChannel_Client_t *c, MopChannel_ChannelEvent_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* MOP_CHANNEL_CLIENT_H */

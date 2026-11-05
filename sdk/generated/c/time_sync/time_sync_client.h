/**
 * @file    time_sync_client.h
 * @brief   TimeSync client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef TIME_SYNC_CLIENT_H
#define TIME_SYNC_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "time_sync_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TimeSync_Client TimeSync_Client_t;

TimeSync_Client_t *TimeSync_CreateClient (ThingModelClientSession_t *session);
void              TimeSync_DestroyClient(TimeSync_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *TimeSync_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *TimeSync_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */

T_ThingModelReturnCode TimeSync_SetSignalMode(TimeSync_Client_t *c, const T_TimeSyncSetSignalModeRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode TimeSync_SetSignalModeAsync(TimeSync_Client_t *c, const T_TimeSyncSetSignalModeRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *TimeSync_BuildSetSignalModePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_TimeSyncSetSignalModeRequest *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*TimeSync_Sync_Cb)(void *user, const T_TimeSyncTimeSyncPush *push);
T_ThingModelReturnCode TimeSync_OnSync(TimeSync_Client_t *c, TimeSync_Sync_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_CLIENT_H */

/**
 * @file    data_subscription_client.h
 * @brief   DataSubscription client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef DATA_SUBSCRIPTION_CLIENT_H
#define DATA_SUBSCRIPTION_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "data_subscription_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DataSubscription_Client DataSubscription_Client_t;

DataSubscription_Client_t *DataSubscription_CreateClient (ThingModelClientSession_t *session);
void              DataSubscription_DestroyClient(DataSubscription_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *DataSubscription_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *DataSubscription_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */

T_ThingModelReturnCode DataSubscription_SubscribeTopic(DataSubscription_Client_t *c, const T_DataSubscriptionSubscribeTopicRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode DataSubscription_SubscribeTopicAsync(DataSubscription_Client_t *c, const T_DataSubscriptionSubscribeTopicRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *DataSubscription_BuildSubscribeTopicPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_DataSubscriptionSubscribeTopicRequest *req);
T_ThingModelReturnCode DataSubscription_UnsubscribeTopic(DataSubscription_Client_t *c, const T_DataSubscriptionUnsubscribeTopicRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode DataSubscription_UnsubscribeTopicAsync(DataSubscription_Client_t *c, const T_DataSubscriptionUnsubscribeTopicRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *DataSubscription_BuildUnsubscribeTopicPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_DataSubscriptionUnsubscribeTopicRequest *req);
T_ThingModelReturnCode DataSubscription_GetLatestValue(DataSubscription_Client_t *c, const T_DataSubscriptionGetLatestValueRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode DataSubscription_GetLatestValueAsync(DataSubscription_Client_t *c, const T_DataSubscriptionGetLatestValueRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *DataSubscription_BuildGetLatestValuePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_DataSubscriptionGetLatestValueRequest *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*DataSubscription_PublishData_Cb)(void *user, const T_DataSubscriptionPublishDataPush *push);
T_ThingModelReturnCode DataSubscription_OnPublishData(DataSubscription_Client_t *c, DataSubscription_PublishData_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_CLIENT_H */

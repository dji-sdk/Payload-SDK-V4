/**
 * @file    data_subscription_impl.h
 * @brief   DataSubscription server-side implementation (impl) API: the device side registers handlers to receive cloud→device calls.
 *
 * Symmetric with the client side: the client calls, impl implements and returns a reply.
 * Covers the three categories SERVICE + PROPERTIES + PROPERTIES_READONLY (EVENT/OSD/STATE are one-way device→cloud pushes,
 * which need no impl-side handler — the device side just uses the Publisher API to send them directly).
 * Handlers run on the transport thread: do not call this cap's or the client's sync API inside them, and do not Destroy.
 */
#ifndef DATA_SUBSCRIPTION_IMPL_H
#define DATA_SUBSCRIPTION_IMPL_H

#include "thing_model_client/thing_model_client_session.h"
#include "data_subscription_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DataSubscriptionImpl DataSubscriptionImpl_t;

/**
 * Create impl: subscribes to the services / property/get / property/set topics and is ready to receive cloud→device calls.
 * @param session   an already-Connected session
 * @param device_id which device's topic to listen on (usually = cfg.device_sn, may differ — used for proxy/testing)
 */
DataSubscriptionImpl_t *DataSubscriptionImpl_Create (ThingModelClientSession_t *session, const char *device_id);
void               DataSubscriptionImpl_Destroy(DataSubscriptionImpl_t *impl);

/** Protocol code version this SDK was built against. */
const char *DataSubscriptionImpl_GeneratorVersion(void);

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
/** SubscribeTopic handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*DataSubscriptionImpl_SubscribeTopic_Handler)(
    DataSubscriptionImpl_t *impl, void *user,
    const T_DataSubscriptionSubscribeTopicRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode DataSubscriptionImpl_OnSubscribeTopic(DataSubscriptionImpl_t *impl, DataSubscriptionImpl_SubscribeTopic_Handler h, void *user);
/** UnsubscribeTopic handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*DataSubscriptionImpl_UnsubscribeTopic_Handler)(
    DataSubscriptionImpl_t *impl, void *user,
    const T_DataSubscriptionUnsubscribeTopicRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode DataSubscriptionImpl_OnUnsubscribeTopic(DataSubscriptionImpl_t *impl, DataSubscriptionImpl_UnsubscribeTopic_Handler h, void *user);
/** GetLatestValue handler — reply type T_ThingModelCommonReply (CommonResponse) */
typedef T_ThingModelReturnCode (*DataSubscriptionImpl_GetLatestValue_Handler)(
    DataSubscriptionImpl_t *impl, void *user,
    const T_DataSubscriptionGetLatestValueRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode DataSubscriptionImpl_OnGetLatestValue(DataSubscriptionImpl_t *impl, DataSubscriptionImpl_GetLatestValue_Handler h, void *user);


#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_IMPL_H */

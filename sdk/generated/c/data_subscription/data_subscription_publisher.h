/**
 * @file    data_subscription_publisher.h
 * @brief   DataSubscription publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef DATA_SUBSCRIPTION_PUBLISHER_H
#define DATA_SUBSCRIPTION_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "data_subscription_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DataSubscription_Publisher DataSubscription_Publisher_t;

DataSubscription_Publisher_t *DataSubscription_CreatePublisher (ThingModelClientSession_t *session);
void                              DataSubscription_DestroyPublisher(DataSubscription_Publisher_t *p);

/** Publish PublishData (osd topic). */
T_ThingModelReturnCode DataSubscription_PublishPublishData(DataSubscription_Publisher_t *p, const T_DataSubscriptionPublishDataPush *push);

#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_PUBLISHER_H */

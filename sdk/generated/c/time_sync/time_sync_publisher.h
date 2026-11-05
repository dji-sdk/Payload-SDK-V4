/**
 * @file    time_sync_publisher.h
 * @brief   TimeSync publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef TIME_SYNC_PUBLISHER_H
#define TIME_SYNC_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "time_sync_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TimeSync_Publisher TimeSync_Publisher_t;

TimeSync_Publisher_t *TimeSync_CreatePublisher (ThingModelClientSession_t *session);
void                              TimeSync_DestroyPublisher(TimeSync_Publisher_t *p);

/** Publish Sync (event topic). */
T_ThingModelReturnCode TimeSync_PublishSync(TimeSync_Publisher_t *p, const T_TimeSyncTimeSyncPush *push);

#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_PUBLISHER_H */

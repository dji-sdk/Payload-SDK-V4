/**
 * @file    custom_small_data_publisher.h
 * @brief   CustomSmallData publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef CUSTOM_SMALL_DATA_PUBLISHER_H
#define CUSTOM_SMALL_DATA_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "custom_small_data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CustomSmallData_Publisher CustomSmallData_Publisher_t;

CustomSmallData_Publisher_t *CustomSmallData_CreatePublisher (ThingModelClientSession_t *session);
void                              CustomSmallData_DestroyPublisher(CustomSmallData_Publisher_t *p);

/** Publish Transmit (event topic). */
T_ThingModelReturnCode CustomSmallData_PublishTransmit(CustomSmallData_Publisher_t *p, const T_TransmitRequest *push);
/** Publish TransmitPush (event topic). */
T_ThingModelReturnCode CustomSmallData_PublishTransmitPush(CustomSmallData_Publisher_t *p, const T_TransmitEventPush *push);

#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_PUBLISHER_H */

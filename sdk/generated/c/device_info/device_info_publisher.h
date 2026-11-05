/**
 * @file    device_info_publisher.h
 * @brief   DeviceInfo publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef DEVICE_INFO_PUBLISHER_H
#define DEVICE_INFO_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "device_info_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DeviceInfo_Publisher DeviceInfo_Publisher_t;

DeviceInfo_Publisher_t *DeviceInfo_CreatePublisher (ThingModelClientSession_t *session);
void                              DeviceInfo_DestroyPublisher(DeviceInfo_Publisher_t *p);

/** Publish Info (state topic). */
T_ThingModelReturnCode DeviceInfo_PublishInfo(DeviceInfo_Publisher_t *p, const T_DeviceInfoPush *push);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_INFO_PUBLISHER_H */

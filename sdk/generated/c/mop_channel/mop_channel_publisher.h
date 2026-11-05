/**
 * @file    mop_channel_publisher.h
 * @brief   MopChannel publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef MOP_CHANNEL_PUBLISHER_H
#define MOP_CHANNEL_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "mop_channel_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MopChannel_Publisher MopChannel_Publisher_t;

MopChannel_Publisher_t *MopChannel_CreatePublisher (ThingModelClientSession_t *session);
void                              MopChannel_DestroyPublisher(MopChannel_Publisher_t *p);

/** Publish ChannelEvent (event topic). */
T_ThingModelReturnCode MopChannel_PublishChannelEvent(MopChannel_Publisher_t *p, const T_ChannelEventPush *push);

#ifdef __cplusplus
}
#endif
#endif /* MOP_CHANNEL_PUBLISHER_H */

/**
 * @file    power_publisher.h
 * @brief   Power publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef POWER_PUBLISHER_H
#define POWER_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "power_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Power_Publisher Power_Publisher_t;

Power_Publisher_t *Power_CreatePublisher (ThingModelClientSession_t *session);
void                              Power_DestroyPublisher(Power_Publisher_t *p);


#ifdef __cplusplus
}
#endif
#endif /* POWER_PUBLISHER_H */

/**
 * @file    efts_publisher.h
 * @brief   Efts publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef EFTS_PUBLISHER_H
#define EFTS_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "efts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Efts_Publisher Efts_Publisher_t;

Efts_Publisher_t *Efts_CreatePublisher (ThingModelClientSession_t *session);
void                              Efts_DestroyPublisher(Efts_Publisher_t *p);


#ifdef __cplusplus
}
#endif
#endif /* EFTS_PUBLISHER_H */

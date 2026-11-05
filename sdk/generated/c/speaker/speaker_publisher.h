/**
 * @file    speaker_publisher.h
 * @brief   Speaker publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef SPEAKER_PUBLISHER_H
#define SPEAKER_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "speaker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Speaker_Publisher Speaker_Publisher_t;

Speaker_Publisher_t *Speaker_CreatePublisher (ThingModelClientSession_t *session);
void                              Speaker_DestroyPublisher(Speaker_Publisher_t *p);

/** Publish State (state topic). */
T_ThingModelReturnCode Speaker_PublishState(Speaker_Publisher_t *p, const T_SpeakerStatePush *push);

#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_PUBLISHER_H */

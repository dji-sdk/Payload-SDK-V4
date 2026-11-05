/**
 * @file    upgrade_publisher.h
 * @brief   Upgrade publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef UPGRADE_PUBLISHER_H
#define UPGRADE_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "upgrade_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Upgrade_Publisher Upgrade_Publisher_t;

Upgrade_Publisher_t *Upgrade_CreatePublisher (ThingModelClientSession_t *session);
void                              Upgrade_DestroyPublisher(Upgrade_Publisher_t *p);

/** Publish PushUpgradeState (event topic). */
T_ThingModelReturnCode Upgrade_PublishPushUpgradeState(Upgrade_Publisher_t *p, const T_UpgradeStatePush *push);

#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_PUBLISHER_H */

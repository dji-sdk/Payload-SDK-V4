/**
 * @file    custom_small_data_client.h
 * @brief   CustomSmallData client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef CUSTOM_SMALL_DATA_CLIENT_H
#define CUSTOM_SMALL_DATA_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "custom_small_data_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CustomSmallData_Client CustomSmallData_Client_t;

CustomSmallData_Client_t *CustomSmallData_CreateClient (ThingModelClientSession_t *session);
void              CustomSmallData_DestroyClient(CustomSmallData_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *CustomSmallData_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *CustomSmallData_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);





/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*CustomSmallData_Transmit_Cb)(void *user, const T_TransmitRequest *push);
T_ThingModelReturnCode CustomSmallData_OnTransmit(CustomSmallData_Client_t *c, CustomSmallData_Transmit_Cb cb, void *user);
typedef void (*CustomSmallData_TransmitPush_Cb)(void *user, const T_TransmitEventPush *push);
T_ThingModelReturnCode CustomSmallData_OnTransmitPush(CustomSmallData_Client_t *c, CustomSmallData_TransmitPush_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_CLIENT_H */

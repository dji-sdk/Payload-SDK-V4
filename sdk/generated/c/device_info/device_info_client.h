/**
 * @file    device_info_client.h
 * @brief   DeviceInfo client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef DEVICE_INFO_CLIENT_H
#define DEVICE_INFO_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "device_info_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DeviceInfo_Client DeviceInfo_Client_t;

DeviceInfo_Client_t *DeviceInfo_CreateClient (ThingModelClientSession_t *session);
void              DeviceInfo_DestroyClient(DeviceInfo_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *DeviceInfo_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *DeviceInfo_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);





/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*DeviceInfo_Info_Cb)(void *user, const T_DeviceInfoPush *push);
T_ThingModelReturnCode DeviceInfo_OnInfo(DeviceInfo_Client_t *c, DeviceInfo_Info_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_INFO_CLIENT_H */

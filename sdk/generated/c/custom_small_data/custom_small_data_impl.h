/**
 * @file    custom_small_data_impl.h
 * @brief   CustomSmallData server-side implementation (impl) API: the device side registers handlers to receive cloud→device calls.
 *
 * Symmetric with the client side: the client calls, impl implements and returns a reply.
 * Covers the three categories SERVICE + PROPERTIES + PROPERTIES_READONLY (EVENT/OSD/STATE are one-way device→cloud pushes,
 * which need no impl-side handler — the device side just uses the Publisher API to send them directly).
 * Handlers run on the transport thread: do not call this cap's or the client's sync API inside them, and do not Destroy.
 */
#ifndef CUSTOM_SMALL_DATA_IMPL_H
#define CUSTOM_SMALL_DATA_IMPL_H

#include "thing_model_client/thing_model_client_session.h"
#include "custom_small_data_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CustomSmallDataImpl CustomSmallDataImpl_t;

/**
 * Create impl: subscribes to the services / property/get / property/set topics and is ready to receive cloud→device calls.
 * @param session   an already-Connected session
 * @param device_id which device's topic to listen on (usually = cfg.device_sn, may differ — used for proxy/testing)
 */
CustomSmallDataImpl_t *CustomSmallDataImpl_Create (ThingModelClientSession_t *session, const char *device_id);
void               CustomSmallDataImpl_Destroy(CustomSmallDataImpl_t *impl);

/** Protocol code version this SDK was built against. */
const char *CustomSmallDataImpl_GeneratorVersion(void);



#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_IMPL_H */

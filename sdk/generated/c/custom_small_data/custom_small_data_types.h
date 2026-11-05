/**
 * @file    custom_small_data_types.h
 * @brief   CustomSmallData enums + structs + init functions (DO NOT EDIT).
 */
#ifndef CUSTOM_SMALL_DATA_TYPES_H
#define CUSTOM_SMALL_DATA_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  E_ChannelAddress   target;
  T_ThingModelBytes  payload;
} T_TransmitRequest;

typedef struct {
  E_ChannelAddress   source;
  T_ThingModelBytes  payload;
} T_TransmitEventPush;


static inline void CustomSmallData_InitTransmitRequest(T_TransmitRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void CustomSmallData_InitTransmitEventPush(T_TransmitEventPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_TYPES_H */

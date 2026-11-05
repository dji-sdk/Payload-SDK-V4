/**
 * @file    power_types.h
 * @brief   Power enums + structs + init functions (DO NOT EDIT).
 */
#ifndef POWER_TYPES_H
#define POWER_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  int32_t  result;
  char     *result_message;
  bool     preparation_flag;
} T_PowerOffNotificationReply;


static inline void Power_InitPowerOffNotificationReply(T_PowerOffNotificationReply *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* POWER_TYPES_H */

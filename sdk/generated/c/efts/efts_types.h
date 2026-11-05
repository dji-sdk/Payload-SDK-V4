/**
 * @file    efts_types.h
 * @brief   Efts enums + structs + init functions (DO NOT EDIT).
 */
#ifndef EFTS_TYPES_H
#define EFTS_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFTS_FTS_STATUS_NOT_TRIGGERED = 0,
    EFTS_FTS_STATUS_TRIGGERED = 1,
} E_EftsFtsStatus;


typedef struct {
  int32_t         result;
  char            *result_message;
  E_EftsFtsStatus fts_status;
  int32_t         fts_pwm_cnt;
} T_EftsGetFtsTriggerStatusReply;


static inline void Efts_InitEftsGetFtsTriggerStatusReply(T_EftsGetFtsTriggerStatusReply *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* EFTS_TYPES_H */

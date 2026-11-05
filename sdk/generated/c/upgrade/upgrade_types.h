/**
 * @file    upgrade_types.h
 * @brief   Upgrade enums + structs + init functions (DO NOT EDIT).
 */
#ifndef UPGRADE_TYPES_H
#define UPGRADE_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UPGRADE_STAGE_IDLE = 0,
    UPGRADE_STAGE_UPGRADING = 3,
    UPGRADE_STAGE_COMPLETE = 4,
    UPGRADE_STAGE_REBOOTING = 6,
} E_UpgradeStage;


typedef struct {
  int32_t  result;
  char     *result_message;
  int32_t  wait_time;
  char     *url;
} T_UpgradeEnterModeReply;

typedef struct {
  int32_t upgrade_stage;
  int32_t value;
} T_UpgradeStatePush;


static inline void Upgrade_InitUpgradeEnterModeReply(T_UpgradeEnterModeReply *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Upgrade_InitUpgradeStatePush(T_UpgradeStatePush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_TYPES_H */

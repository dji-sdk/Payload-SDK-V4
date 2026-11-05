#ifndef UPGRADE_RTOS_STORAGE_H
#define UPGRADE_RTOS_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

int UpgradeRtosStorage_Begin(uint32_t expected_size);
int UpgradeRtosStorage_PreErase(void);
int UpgradeRtosStorage_Append(const uint8_t *data, uint32_t len);
int UpgradeRtosStorage_Finish(void);
bool UpgradeRtosStorage_IsReady(void);
uint32_t UpgradeRtosStorage_Size(void);
int UpgradeRtosStorage_SetBootPending(void);
bool UpgradeRtosStorage_ConsumeBootPending(void);
bool UpgradeRtosStorage_HasPendingCompletion(void);
int UpgradeRtosStorage_MarkCompletionFinished(void);

#endif

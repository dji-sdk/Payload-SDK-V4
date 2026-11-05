/**
 * @file    time_sync_types.h
 * @brief   TimeSync enums + structs + init functions (DO NOT EDIT).
 */
#ifndef TIME_SYNC_TYPES_H
#define TIME_SYNC_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TIME_SYNC_SIGNAL_MODE_FTS = 0,
    TIME_SYNC_SIGNAL_MODE_PPS = 1,
} E_TimeSyncSignalMode;


typedef struct {
  E_TimeSyncSignalMode mode;
} T_TimeSyncSetSignalModeRequest;

typedef struct {
  int32_t year;
  int32_t month;
  int32_t day;
  int32_t hour;
  int32_t minute;
  int32_t second;
  int32_t nano_second;
} T_TimeSyncTimeSyncPush;


static inline void TimeSync_InitTimeSyncSetSignalModeRequest(T_TimeSyncSetSignalModeRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void TimeSync_InitTimeSyncTimeSyncPush(T_TimeSyncTimeSyncPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_TYPES_H */

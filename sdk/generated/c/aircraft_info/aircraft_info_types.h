/**
 * @file    aircraft_info_types.h
 * @brief   AircraftInfo enums + structs + init functions (DO NOT EDIT).
 */
#ifndef AIRCRAFT_INFO_TYPES_H
#define AIRCRAFT_INFO_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  int32_t language;
  int32_t screen_type;
} T_MobileAppInfo;

typedef struct {
  char     *device_id;
  char     *device_name;
  char     *serial_number;
  char     *firmware_version;
  int32_t  device_type;
} T_AircraftDeviceInfo;

typedef struct {
  int32_t              aircraft_type;
  char                 *aircraft_version;
  T_MobileAppInfo      mobile_app_info;
  T_AircraftDeviceInfo *devices_info;
  size_t               devices_info_count;
} T_AircraftInfoPush;


static inline void AircraftInfo_InitMobileAppInfo(T_MobileAppInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void AircraftInfo_InitAircraftDeviceInfo(T_AircraftDeviceInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void AircraftInfo_InitAircraftInfoPush(T_AircraftInfoPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* AIRCRAFT_INFO_TYPES_H */

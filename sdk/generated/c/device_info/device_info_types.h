/**
 * @file    device_info_types.h
 * @brief   DeviceInfo enums + structs + init functions (DO NOT EDIT).
 */
#ifndef DEVICE_INFO_TYPES_H
#define DEVICE_INFO_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  char     *device_id;
  char     *device_name;
  char     *serial_number;
  char     *firmware_version;
  int32_t  device_type;
  char     **capabilities;
  size_t   capabilities_count;
} T_DeviceInfoPush;


static inline void DeviceInfo_InitDeviceInfoPush(T_DeviceInfoPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_INFO_TYPES_H */

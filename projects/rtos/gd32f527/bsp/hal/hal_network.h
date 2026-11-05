/**
 ********************************************************************
 * @file    hal_network.h
 * @brief   Header file for GD32 RNDIS network HAL implementation.
 *
 * @copyright (c) 2021 DJI. All rights reserved.
 *
 *********************************************************************
 */

#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include "dji_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RNDIS USB device VID/PID (from rndis_core.c) */
#define USB_NET_ADAPTER_VID    (0x28E9U)
#define USB_NET_ADAPTER_PID    (0x0801U)

/* Exported functions */
T_DjiReturnCode HalNetWork_Init(const char *ipAddr, const char *netMask, T_DjiNetworkHandle *halObj);
T_DjiReturnCode HalNetWork_DeInit(T_DjiNetworkHandle halObj);
T_DjiReturnCode HalNetWork_GetDeviceInfo(T_DjiHalNetworkDeviceInfo *deviceInfo);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NETWORK_H */
/****************** (C) COPYRIGHT DJI Innovations *****END OF FILE****/

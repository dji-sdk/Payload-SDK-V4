/**
 ********************************************************************
 * @file    hal_network.c
 * @brief   GD32 RNDIS network HAL implementation.
 *          Replaces Linux ifconfig with lwIP netif_set_addr().
 *
 * @copyright (c) 2021 DJI. All rights reserved.
 *
 *********************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "stdio.h"
#include "hal_network.h"
#include "dji_logger.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "netconf.h"

/* Private constants ---------------------------------------------------------*/
#define TAG "hal_network"

/* Exported functions definition ---------------------------------------------*/

/**
 * @brief  Configure RNDIS network interface IP address and netmask.
 *         Called by PSDK during aircraft identification (probes 7 candidate IPs).
 * @param  ipAddr   IP address string, e.g. "192.0.2.2"
 * @param  netMask  Netmask string,    e.g. "255.255.0.0"
 * @param  halObj   Not used on GD32 (no persistent handle needed)
 */
T_DjiReturnCode HalNetWork_Init(const char *ipAddr, const char *netMask,
                                T_DjiNetworkHandle *halObj)
{
    ip4_addr_t ip, mask, gw;

    if (ipAddr == NULL || netMask == NULL) {
        USER_LOG_ERROR(TAG, "network init param error");
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    /* Parse IP and netmask strings */
    if (!ip4addr_aton(ipAddr, &ip)) {
        USER_LOG_ERROR(TAG, "invalid ip address: %s", ipAddr);
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    if (!ip4addr_aton(netMask, &mask)) {
        USER_LOG_ERROR(TAG, "invalid netmask: %s", netMask);
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    /* Keep current gateway unchanged */
    ip4_addr_copy(gw, *netif_ip4_gw(&g_mynetif));

    /* Apply new IP/mask to RNDIS netif via lwIP API */
    netif_set_addr(&g_mynetif, &ip, &mask, &gw);

    USER_LOG_INFO(TAG, "netif IP set to %s mask %s", ipAddr, netMask);

    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

/**
 * @brief  De-initialize network interface. Nothing to do on GD32.
 */
T_DjiReturnCode HalNetWork_DeInit(T_DjiNetworkHandle halObj)
{
    (void)halObj;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

/**
 * @brief  Return RNDIS USB adapter VID/PID so PSDK can identify the device.
 */
T_DjiReturnCode HalNetWork_GetDeviceInfo(T_DjiHalNetworkDeviceInfo *deviceInfo)
{
    deviceInfo->usbNetAdapter.vid = USB_NET_ADAPTER_VID;  /* 0x28E9 */
    deviceInfo->usbNetAdapter.pid = USB_NET_ADAPTER_PID;  /* 0x0801 */
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

/****************** (C) COPYRIGHT DJI Innovations *****END OF FILE****/

/**
 ********************************************************************
 * @file    usb_device.c
 * @version V2.0.0
 * @date    2019/9/20
 * @brief
 *
 * @copyright (c) 2021 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI's authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "usb_device.h"
#include "osal.h"
#include "stdio.h"
#include "dji_logger.h"
#include "gd32f5xx.h"
#include "dji_ringbuffer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_usb_hw.h"

#ifdef USE_USB_CDC
#include "cdc_acm_core.h"
#elif defined(USE_USB_RNDIS)
#include "rndis_core.h"
#include "netconf.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#endif

/* Private constants ---------------------------------------------------------*/
#define USB_DEVICE_READ_BUF_SIZE     4096
#define USB_DEVICE_WRITE_BUF_SIZE    4096
#define USB_DEVICE_READ_DATA_SIZE    1024

/* Private types -------------------------------------------------------------*/

/* Private values -------------------------------------------------------------*/
static T_RingBuffer s_usbDeviceReadRingBuffer;
static T_RingBuffer s_usbDeviceWriteRingBuffer;
static uint8_t s_usbDeviceReadBuf[USB_DEVICE_READ_BUF_SIZE];
static uint8_t s_usbDeviceWriteBuf[USB_DEVICE_WRITE_BUF_SIZE];
static T_DjiMutexHandle s_usbDeviceMutex;
usb_core_driver usb_dev0;

/* Private functions declaration ---------------------------------------------*/

/* Exported functions definition ---------------------------------------------*/
T_DjiReturnCode USBD_DEV_Init(void)
{
    RingBuf_Init(&s_usbDeviceReadRingBuffer, s_usbDeviceReadBuf, USB_DEVICE_READ_BUF_SIZE);
    RingBuf_Init(&s_usbDeviceWriteRingBuffer, s_usbDeviceWriteBuf, USB_DEVICE_WRITE_BUF_SIZE);

    usb_gpio_config();
    usb_rcu_config();
    usb_timer_init();

#ifdef USE_USB_CDC
    usbd_init(&usb_dev0,
#ifdef USE_USB_FS
              USB_CORE_ENUM_FS,
#elif defined(USE_USB_HS)
              USB_CORE_ENUM_HS,
#endif /* USE_USB_FS */
              &cdc_desc,
              &cdc_class);
#elif defined(USE_USB_RNDIS)
    usbd_init(&usb_dev0,
#ifdef USE_USB_FS
              USB_CORE_ENUM_FS,
#elif defined(USE_USB_HS)
              USB_CORE_ENUM_HS,
#endif /* USE_USB_FS */
              &rndis_desc,
              &rndis_class);
#endif /* USE_USB_CDC / USE_USB_RNDIS */

    usb_intr_config();

    Osal_MutexCreate(&s_usbDeviceMutex);

    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode USBD_DEV_DeInit(void)
{
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode USBD_CDC_ReadData(uint8_t *buf, uint32_t len, uint32_t *realLen)
{
    Osal_MutexLock(s_usbDeviceMutex);
    *realLen = RingBuf_Get(&s_usbDeviceReadRingBuffer, buf, len);
    Osal_MutexUnlock(s_usbDeviceMutex);

    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode USBD_CDC_WriteData(const uint8_t *buf, uint32_t len, uint32_t *realLen)
{
#ifdef USE_USB_CDC
    uint32_t blockSize = USB_CDC_DATA_PACKET_SIZE;
    uint32_t blockNum = len / blockSize;
    uint32_t blockRemain = len % blockSize;
    uint32_t sentLen = 0;
    *realLen = 0;

    for (uint32_t i = 0; i < blockNum; i++) {
        cdc_acm_data_send(&usb_dev0, &buf[i * blockSize], blockSize, &sentLen);
        *realLen += sentLen;
    }

    if (blockRemain > 0) {
        cdc_acm_data_send(&usb_dev0, &buf[blockNum * blockSize], blockRemain, &sentLen);
        *realLen += sentLen;
    }
#else
    *realLen = 0;
#endif
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

void USBD_CDC_Handle(void)
{
#ifdef USE_USB_CDC
    uint16_t realCountPutBuffer = 0;
    uint32_t readRealLen = 0;
    uint8_t readBuf[USB_DEVICE_READ_DATA_SIZE] = {0};

    if (USBD_CONFIGURED != usb_dev0.dev.cur_status) {
        return;
    }

    cdc_acm_data_receive(&usb_dev0, readBuf, USB_DEVICE_READ_DATA_SIZE, &readRealLen);
    if (readRealLen > 0) {
        Osal_MutexLock(s_usbDeviceMutex);
        realCountPutBuffer = RingBuf_Put(&s_usbDeviceReadRingBuffer, readBuf, readRealLen);
        Osal_MutexUnlock(s_usbDeviceMutex);
        if (realCountPutBuffer < readRealLen) {
            USER_LOG_ERROR("failed to put data, %u/%u", realCountPutBuffer, readRealLen);
        }
        USER_LOG_DEBUG("Rececive cdc data finished, dataLen: %d", readRealLen);
    }
#endif /* USE_USB_CDC */
}

#ifdef USE_USB_RNDIS
uint8_t USBD_RNDIS_HasPendingWork(void)
{
    uint8_t pending = 0U;
    /* USB IRQ2 is above the RTOS syscall ceiling; use a brief PRIMASK snapshot. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (USBD_CONFIGURED == usb_dev0.dev.cur_status) {
        const volatile usbd_rndis_handler *handler =
            (const volatile usbd_rndis_handler *)usb_dev0.dev.class_data[USBD_RNDIS_INTERFACE];
        if (handler != NULL) {
            pending = (handler->usb_to_enet.USB_enet_rx_flag != 0U ||
                       handler->usb_enet_cmd.cmd_send != 0U ||
                       handler->usb_enet_cmd.cmd_reset != 0U);
        }
    }
    __set_PRIMASK(primask);
    return pending;
}

void USBD_RNDIS_Handle(void)
{
    if (USBD_CONFIGURED != usb_dev0.dev.cur_status) return;

    usbd_rndis_handler *rndis_handler =
        (usbd_rndis_handler *)usb_dev0.dev.class_data[USBD_RNDIS_INTERFACE];

    if (1U == rndis_handler->usb_enet_cmd.cmd_send) {
        rndis_handler->usb_enet_cmd.cmd_send = 0U;
        rndis_cmd_send(&usb_dev0, 8U);
    }

    /* The host can reset the RNDIS data path without requesting USB
     * re-enumeration. The RESET completion is sent in rndis_core.c; keep the
     * USB device attached or the host treats this as a payload USB loss. */
    if (1U == rndis_handler->usb_enet_cmd.cmd_reset) {
        printf("[RNDIS] host RESET completed; keeping USB attached\r\n");
        rndis_handler->usb_enet_cmd.cmd_reset = 0U;
    }

    /* lwIP polling */
    lwip_frame_recv();
    lwip_timeouts_check(sys_now());
}

T_DjiReturnCode USBD_RNDIS_IsReady(void)
{
    return (USBD_CONFIGURED == usb_dev0.dev.cur_status) ? 1U : 0U;
}

#endif /* USE_USB_RNDIS */

/* Private functions definition-----------------------------------------------*/

/****************** (C) COPYRIGHT DJI Innovations *****END OF FILE****/

/*!
    ile    usbd_conf.h
    rief   the header file of USB device configuration

    ersion 2023-12-20, V1.0.0, firmware for GD32F5xx
*/

/*
    Copyright (c) 2023, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#include "usb_conf.h"

#define USBD_CFG_MAX_NUM        1
#define USBD_ITF_MAX_NUM        1
#define USB_STR_DESC_MAX_SIZE   255
#define USB_STRING_COUNT        4

/* USB device option */
#ifdef USE_USB_CDC

#define APP_RX_DATA_SIZE        2048
#define CDC_COM_INTERFACE       0
#define CDC_DATA_IN_EP          EP1_IN
#define CDC_DATA_OUT_EP         EP3_OUT
#define CDC_CMD_EP              EP2_IN
#define USB_CDC_CMD_PACKET_SIZE 8

#ifdef USE_USB_HS
    #ifdef USE_ULPI_PHY
        #define USB_CDC_DATA_PACKET_SIZE    512
        #define CDC_IN_FRAME_INTERVAL       40
    #else
        #define USB_CDC_DATA_PACKET_SIZE    64
        #define CDC_IN_FRAME_INTERVAL       5
    #endif
#else
    #define USB_CDC_DATA_PACKET_SIZE        64
    #define CDC_IN_FRAME_INTERVAL           5
#endif /* USE_USB_FS */

#elif defined(USE_USB_RNDIS)

#define USBD_RNDIS_INTERFACE         0U
#define RNDIS_DATA_IN_EP             EP1_IN
#define RNDIS_DATA_OUT_EP            EP3_OUT
#define RNDIS_CMD_EP                 EP2_IN
#define USB_RNDIS_CMD_PACKET_SIZE    8U

#ifdef USE_USB_HS
    #ifdef USE_ULPI_PHY
        #define USB_RNDIS_DATA_PACKET_SIZE    512U
        #define RNDIS_IN_FRAME_INTERVAL       40U
    #else
        #define USB_RNDIS_DATA_PACKET_SIZE    64U
        #define RNDIS_IN_FRAME_INTERVAL       5U
    #endif
#else
    #define USB_RNDIS_DATA_PACKET_SIZE        64U
    #define RNDIS_IN_FRAME_INTERVAL           5U
#endif /* USE_USB_FS */

#endif /* USB device option */

#endif /* __USBD_CONF_H */

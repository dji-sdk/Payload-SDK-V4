/*!
    File    usb_rndis_config.h
    Brief   network configuration for USB RNDIS + lwIP + MQTT
*/

#ifndef USB_RNDIS_CONFIG_H
#define USB_RNDIS_CONFIG_H

#include "gd32f5xx.h"

/* Device MAC address */
#define BOARD_MAC_ADDR0      0x02
#define BOARD_MAC_ADDR1      0x0A
#define BOARD_MAC_ADDR2      0xF7
#define BOARD_MAC_ADDR3      0xE3
#define BOARD_MAC_ADDR4      0xD5
#define BOARD_MAC_ADDR5      0x42

/* Device static IP, configured by BOARD_IP_ADDR0..3. */
#define BOARD_IP_ADDR0       192
#define BOARD_IP_ADDR1       168
#define BOARD_IP_ADDR2       130//200
#define BOARD_IP_ADDR3       2//77

/* Netmask: 255.255.255.0 */
#define BOARD_NETMASK_ADDR0  255
#define BOARD_NETMASK_ADDR1  255
#define BOARD_NETMASK_ADDR2  255//255
#define BOARD_NETMASK_ADDR3  0

/* Gateway, configured by BOARD_GW_ADDR0..3. */
#define BOARD_GW_ADDR0       192
#define BOARD_GW_ADDR1       168
#define BOARD_GW_ADDR2       130//200
#define BOARD_GW_ADDR3       1

/* MQTT broker IP, configured by IP_D_ADDR0..3. */
#define IP_D_ADDR0           192
#define IP_D_ADDR1           168
#define IP_D_ADDR2           100
#define IP_D_ADDR3           3


#define USE_DHCP  

#endif /* USB_RNDIS_CONFIG_H */

/*!
    \file    netconf.c
    \brief   network connection configuration

    \version 2025-08-08, V1.3.0, firmware for GD32E51x
*/

/*
    Copyright (c) 2025, GigaDevice Semiconductor Inc.

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

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include "stdint.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "usb_rndis_config.h"
#include "netconf.h"
#include <stdio.h>
#include "lwip/timeouts.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcpip.h"        /* tcpip_init / tcpip_input / LOCK_TCPIP_CORE */

#define DHCP_TRIES_MAX_TIMES        3


#ifdef USE_DHCP
uint32_t finecurtime = 0;
uint32_t coarsecurtime = 0;
dhcp_addr_status_enum dhcp_addr_status = DHCP_ADDR_NONE;
#endif /* USE_DHCP */

struct netif g_mynetif;
uint32_t tcpcurtime = 0;
uint32_t arpcurtime = 0;
ip_addr_t ip_address = {0};

void lwip_dhcp_address_get(void);

/* tcpip_init() starts tcpip_thread (a FreeRTOS task); once that task runs it calls back into
 * tcpip_init_done(). In the callback we complete the netif add/up/dhcp_start, and wake the
 * caller (bsp_init_task) via a semaphore. This avoids touching lwIP before tcpip_thread is up. */
static SemaphoreHandle_t s_lwip_ready_sem;

static void tcpip_init_done(void *arg)
{
    ip_addr_t gd_ipaddr;
    ip_addr_t gd_netmask;
    ip_addr_t gd_gw;

    (void)arg;

#ifdef USE_DHCP
    gd_ipaddr.addr = 0;
    gd_netmask.addr = 0;
    gd_gw.addr = 0;
#else
    IP4_ADDR(&gd_ipaddr, BOARD_IP_ADDR0, BOARD_IP_ADDR1, BOARD_IP_ADDR2, BOARD_IP_ADDR3);
    IP4_ADDR(&gd_netmask, BOARD_NETMASK_ADDR0, BOARD_NETMASK_ADDR1, BOARD_NETMASK_ADDR2, BOARD_NETMASK_ADDR3);
    IP4_ADDR(&gd_gw, BOARD_GW_ADDR0, BOARD_GW_ADDR1, BOARD_GW_ADDR2, BOARD_GW_ADDR3);
#endif /* USE_DHCP */

    /* netif input = tcpip_input:
     *   ethernetif_input() internally goes netif->input(p, netif) -> tcpip_input(...),
     *   which posts the pbuf to tcpip_mbox; tcpip_thread dequeues it and calls ethernet_input
     *   (splitting/dispatching ARP/IP). This way ethernetif_input can be called from any task (we call it in
     *   bsp_init_task). */
    netif_add(&g_mynetif, &gd_ipaddr, &gd_netmask, &gd_gw, NULL, &ethernetif_init, &tcpip_input);

    netif_set_default(&g_mynetif);
    netif_set_status_callback(&g_mynetif, lwip_netif_status_callback);
    netif_set_link_up(&g_mynetif);
    netif_set_up(&g_mynetif);

    /* We do *not* dhcp_start immediately here: keep the original state machine driven by
     * lwip_dhcp_address_get(), whose entry is still periodically called by bsp_init_task
     * (incl. LOCK_TCPIP_CORE), behaving the same as NO_SYS=1. */

    xSemaphoreGive(s_lwip_ready_sem);
}

/*!
    \brief      initializes the LwIP stack (NO_SYS=0)

    NO_SYS=0 initialization flow:
      - mem_init / memp_init / lwip_init / sys_timeouts_init are all done automatically inside
        tcpip_init(); the caller no longer needs to call them manually
      - netif registration is placed in the tcpip_init_done() callback, guaranteeing tcpip_thread is ready by then
      - a binary semaphore waits for the callback to finish before returning; when the caller (bsp_init_task) gets
        control back, netif is already up and it can immediately continue USB packet receive / dhcp driving
    \param[in]  none
    \param[out] none
    \retval     none
*/
void lwip_stack_init(void)
{
    s_lwip_ready_sem = xSemaphoreCreateBinary();
    LWIP_ASSERT("lwip_stack_init: sem create failed", s_lwip_ready_sem != NULL);

    /* tcpip_init() will xTaskCreate("tcpip", TCPIP_THREAD_STACKSIZE, TCPIP_THREAD_PRIO, ...).
     * After its internal setup it calls the tcpip_init_done() callback -> in which we add netif + give sem. */
    tcpip_init(tcpip_init_done, NULL);

    /* wait for the callback to finish (worst-case ~10ms, FreeRTOS task startup + tcpip_thread entering main loop) */
    xSemaphoreTake(s_lwip_ready_sem, portMAX_DELAY);

    vSemaphoreDelete(s_lwip_ready_sem);
    s_lwip_ready_sem = NULL;
}

/*!
    \brief      called when a frame is received from the interface (USB-RNDIS poll context)

    Receive path (NO_SYS=0):
      USB poll -> lwip_frame_recv() -> ethernetif_input()
        -> low_level_input() (pbuf_alloc + memcpy)
        -> netif->input = tcpip_input -> post to tcpip_mbox -> tcpip_thread dequeues
        -> ethernet_input -> ip4_input -> tcp_input -> mqtt callback
    \param[in]  none
    \param[out] none
    \retval     none
*/
void lwip_frame_recv(void)
{
    /* get frame from the interface and pass it to the LwIP stack */
    ethernetif_input(&g_mynetif);
}

/*!
    \brief      no-op hook — under NO_SYS=0 tcpip_thread runs sys_check_timeouts itself

    Under NO_SYS=0, tcpip_thread automatically calls sys_check_timeouts() when sys_arch_mbox_fetch
    returns on timeout, so no application-side driving is needed. This function is kept empty to preserve
    ABI compatibility, keeping the call site in the bsp_init_task main loop unchanged.
    \param[in]  curtime: unused
    \param[out] none
    \retval     none
*/
void lwip_timeouts_check(__IO uint32_t curtime)
{
    (void)curtime;
    /* nothing to do under NO_SYS=0 */
}

#ifdef USE_DHCP
/*!
    \brief      get IP address through DHCP function

    NO_SYS=0: this function is called periodically by bsp_init_task; dhcp_start / dhcp_stop /
    netif_dhcp_data / netif_set_addr all operate on lwIP directly, so the core lock must be held first.
    LOCK_TCPIP_CORE is a recursive mutex under LWIP_TCPIP_CORE_LOCKING=1, so it is safe.
    \param[in]  none
    \param[out] none
    \retval     none
*/
void lwip_dhcp_address_get(void)
{
    ip_addr_t gd_ipaddr;
    ip_addr_t gd_netmask;
    ip_addr_t gd_gw;
    struct dhcp *dhcp_client;

    switch(dhcp_addr_status) {
    case DHCP_ADDR_NONE:
        LOCK_TCPIP_CORE();
        dhcp_start(&g_mynetif);
        UNLOCK_TCPIP_CORE();

        dhcp_addr_status = DHCP_ADDR_BEGIN;
        break;

    case DHCP_ADDR_BEGIN:
        /* reading netif->ip_addr.addr is a 32-bit atomic read, no lock needed */
        ip_address.addr = g_mynetif.ip_addr.addr;

        if(0 != ip_address.addr) {
            dhcp_addr_status = DHCP_ADDR_GOT;

            printf("\r\n[DHCP] eval board ip address: %d.%d.%d.%d \r\n", ip4_addr1_16(&ip_address), \
                   ip4_addr2_16(&ip_address), ip4_addr3_16(&ip_address), ip4_addr4_16(&ip_address));
        } else {
            /* DHCP timeout: reading dhcp_client->tries requires holding the lock, then fallback to static IP */
            LOCK_TCPIP_CORE();
            dhcp_client = netif_dhcp_data(&g_mynetif);
            if(dhcp_client && dhcp_client->tries > DHCP_TRIES_MAX_TIMES) {
                dhcp_addr_status = DHCP_ADDR_FAIL;
                /* stop DHCP */
                dhcp_stop(&g_mynetif);

                /* use static address as IP address */
                IP4_ADDR(&gd_ipaddr, BOARD_IP_ADDR0, BOARD_IP_ADDR1, BOARD_IP_ADDR2, BOARD_IP_ADDR3);
                IP4_ADDR(&gd_netmask, BOARD_NETMASK_ADDR0, BOARD_NETMASK_ADDR1, BOARD_NETMASK_ADDR2, BOARD_NETMASK_ADDR3);
                IP4_ADDR(&gd_gw, BOARD_GW_ADDR0, BOARD_GW_ADDR1, BOARD_GW_ADDR2, BOARD_GW_ADDR3);
                netif_set_addr(&g_mynetif, &gd_ipaddr, &gd_netmask, &gd_gw);
            }
            UNLOCK_TCPIP_CORE();
        }
        break;

    default:
        break;
    }
}
#endif /* USE_DHCP */

/* sys_now() is provided by the lwIP FreeRTOS port (port/GD32F527/FreeRTOS/sys_arch.c),
 * so it is not redefined here, to avoid a multiple-definition link error.
 * The implementation in sys_arch.c matches this file's historical implementation:
 *   return xTaskGetTickCount() * portTICK_PERIOD_MS;  */

/*!
    Brief      callback called when netif status changes (link up/down)
    \param[in]  netif: the network interface
    \param[out] none
    
etval     none
*/
void lwip_netif_status_callback(struct netif *netif)
{
    /* empty callback - add custom logic here if needed */
}

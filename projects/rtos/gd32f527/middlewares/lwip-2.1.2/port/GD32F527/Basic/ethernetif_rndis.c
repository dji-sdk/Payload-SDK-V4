/**
 * @file
 * Ethernet Interface for standalone applications (without RTOS)
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/mem.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "usb_rndis_config.h"
#include <string.h>
#include <stdio.h>
#include "lwip/sys.h"


/* network interface name */
#define IFNAME0 'G'
#define IFNAME1 'D'

/* ENET RxDMA/TxDMA descriptor */
extern enet_descriptors_struct  rxdesc_tab[ENET_RXBUF_NUM], txdesc_tab[ENET_TXBUF_NUM];

/* ENET receive buffer  */
extern uint8_t rx_buff[ENET_RXBUF_NUM][ENET_RXBUF_SIZE]; 

/* ENET transmit buffer */
extern uint8_t tx_buff[ENET_TXBUF_NUM][ENET_TXBUF_SIZE]; 

/*global transmit and receive descriptors pointers */
extern enet_descriptors_struct  *dma_current_txdesc;
extern enet_descriptors_struct  *dma_current_rxdesc;

/* preserve another ENET RxDMA/TxDMA ptp descriptor for normal mode */
enet_descriptors_struct  ptp_txstructure[ENET_TXBUF_NUM];
enet_descriptors_struct  ptp_rxstructure[ENET_RXBUF_NUM];

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
#ifdef CHECKSUM_BY_HARDWARE
    int i;
#endif
    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC hardware address */
    netif->hwaddr[0] =  BOARD_MAC_ADDR0;
    netif->hwaddr[1] =  BOARD_MAC_ADDR1;
    netif->hwaddr[2] =  BOARD_MAC_ADDR2;
    netif->hwaddr[3] =  BOARD_MAC_ADDR3;
    netif->hwaddr[4] =  BOARD_MAC_ADDR4;
    netif->hwaddr[5] =  BOARD_MAC_ADDR5;

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
#include "rndis_core.h"
extern usb_core_driver usb_dev0;

uint8_t Lwip_data_buffer[1530];

/* Threading model: low_level_output is called by tcpip_thread while holding the core lock
 * (LWIP_TCPIP_CORE_LOCKING=1), so only one thread is ever inside it. No extra mutex is needed. */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    int framelength = 0;
    uint8_t *buffer;

    usbd_rndis_handler *rndis_l = (usbd_rndis_handler *)usb_dev0.dev.class_data[USBD_RNDIS_INTERFACE];

    /* class_data is assigned in rndis_init(); it is NULL until RNDIS enumeration completes.
     * Return early: otherwise the NULL deref of rndis_l->packet_sent below makes the volatile read
     * pick up non-zero garbage from flash -> the while loop exits -> enet_data_to_usb uses NULL as
     * its base, reads an arbitrary value at *(NULL+1192) as ENET_receive_length -> memcpy overruns
     * -> BusFault.
     * (main.c::bsp_init_task already waits for USB ready before lwip_stack_init; this check is
     *  defense in depth so an early call in any edge case cannot crash.) */
    if (rndis_l == NULL) {
        return ERR_IF;
    }

    /* Wait for previous USB IN transfer to complete (50 ms cap absorbs
     * worst-case host polling jitter). */
    uint32_t t_wait = sys_now();
    while (rndis_l->usb_to_enet.packet_sent == 0) {
        if (sys_now() - t_wait > 50) {
            uint8_t ep = 1U;  /* RNDIS_DATA_IN_EP = EP1_IN */
            uint32_t diepctl  = usb_dev0.regs.er_in[ep]->DIEPCTL;
            usb_transc *t = &usb_dev0.dev.transc_in[ep];

            /* If hardware actually finished but the class ISR did not
             * set packet_sent (USB FS ZLP edge case), recover instead
             * of dropping. Primary fix is the 1-byte padding in
             * rndis_core.c enet_data_to_usb(). */
            if (((diepctl & 0x80000000U) == 0U) &&
                (t->xfer_count == t->xfer_len)) {
                printf("[RECOVER] USB IN done, force packet_sent=1 (xfer=%lu)\r\n",
                       (unsigned long)t->xfer_len);
                rndis_l->usb_to_enet.packet_sent = 1;
                break;
            }

            printf("[DROP] USB IN busy >50ms (xfer=%lu/%lu)\r\n",
                   (unsigned long)t->xfer_count, (unsigned long)t->xfer_len);
            return ERR_IF;
        }
    }

    buffer = Lwip_data_buffer;

    /* Defensive: skip outsize pbuf chains rather than memcpy past Lwip_data_buffer.
     * Lwip_data_buffer is 1530 B (MTU + 14 + slack). Anything larger is either a
     * misconfigured MTU upstream or a corrupted pbuf chain. */
    if (p->tot_len > sizeof(Lwip_data_buffer)) {
        printf("[ethif] DROP oversized frame tot_len=%u > buf=%u\r\n",
               (unsigned)p->tot_len, (unsigned)sizeof(Lwip_data_buffer));
        return ERR_IF;
    }

    for(q = p; q != NULL; q = q->next){
        memcpy((uint8_t *)&buffer[framelength], q->payload, q->len);
        framelength = framelength + q->len;
    }

    rndis_l->enet_to_usb.ENET_receive_length = framelength;
    enet_data_to_usb(&usb_dev0, buffer);
    return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf * low_level_input(struct netif *netif)
{
    struct pbuf *p, *q;
    u16_t len;
    int l =0;
    uint8_t *buffer;
    
    usbd_rndis_handler *rndis_l = (usbd_rndis_handler *)usb_dev0.dev.class_data[USBD_RNDIS_INTERFACE];

    p = NULL;
    if (rndis_l->usb_to_enet.USB_enet_rx_flag) {
        
        /* obtain the size of the packet and put it into the "len" variable. */
        len = rndis_l->usb_to_enet.USB_enet_data_length;
        
        buffer = (uint8_t *)rndis_l->usb_to_enet.USB_enet_data_buffer;
        
        rndis_l->usb_to_enet.USB_enet_data_length = 0;

        /* we allocate a pbuf chain of pbufs from the Lwip buffer pool */
        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

        /* copy received frame to pbuf chain */
        if (p != NULL){
            for (q = p; q != NULL; q = q->next){ 
                memcpy((uint8_t *)q->payload, (u8_t*)&buffer[l], q->len);
                l = l + q->len;
            }
        }
        
        rndis_l->usb_to_enet.receive_length = 0;
        rndis_l->usb_to_enet.USB_enet_rx_flag = 0;
        rndis_data_receive(&usb_dev0);
    }

    return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
err_t ethernetif_input(struct netif *netif)
{
    err_t err;
    struct pbuf *p;

    /* move received packet into a new pbuf */
    p = low_level_input(netif);

    /* no packet could be read, silently ignore this */
    if (p == NULL) return ERR_MEM;

    /* entry point to the LwIP stack */
    err = netif->input(p, netif);
    
    if (err != ERR_OK){
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
        p = NULL;
    }
    return err;
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
  
#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "Gigadevice.COM_lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

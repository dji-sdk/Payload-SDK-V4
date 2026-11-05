/*
 * upgrade_rtos_http.c — GD32 RTOS HTTP firmware upgrade receiver
 *
 * Approach: fixed-size SRAM ring + dedicated flash writer task.
 *
 * post_auto_wnd=1: lwIP auto-ACKs, the aircraft can send at full speed.
 * httpd_post_receive_data: copy pbuf bytes into a static SRAM ring and
 *                          signal the flash writer, then return. No Flash
 *                          I/O runs on tcpip_thread.
 * flash_writer_task:       drain the ring 4 KB at a time via
 *                          UpgradeRtosStorage_Append, then finalize the
 *                          staging image (SHA-256 verify) off tcpip_thread.
 *
 * Sizing (measured on GD32F527):
 *   A 4 KB Flash page program takes ~34 ms on this driver. The ring hand-off
 *   keeps a steady-state fill of one MSS (1460 B); 16 KB is >10× that
 *   headroom, and no full-image malloc is required. This lets
 *   configTOTAL_HEAP_SIZE drop from 512 KB to 192 KB.
 *
 * If a future change slows Flash writes or increases MSS, the ring size is
 * the single knob to tune (UPG_RING_SIZE below).
 */

#include "upgrade_rtos_http.h"
#include "upgrade_rtos_storage.h"
#include "netconf.h"

#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/tcpip.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include <string.h>

#include "app_widget.h"


#define UPG_RING_SIZE (16U * 1024U)

typedef enum {
    UPG_STATE_IDLE = 0,
    UPG_STATE_RECEIVING,
    UPG_STATE_EOF,      /* tcpip_thread has finished feeding the ring */
    UPG_STATE_DONE,
} upg_state_t;

static int             s_httpd_started;
static int             s_upgrade_prepared;
static uint8_t         s_ring[UPG_RING_SIZE];
static volatile uint32_t s_ring_head;           /* producer (tcpip_thread) */
static volatile uint32_t s_ring_tail;           /* consumer (flash task)   */
static uint32_t        s_expected_size;
static uint32_t        s_produced;              /* bytes written to ring */
static uint32_t        s_consumed;              /* bytes drained from ring */
static volatile upg_state_t s_state;
static int             s_fw_ok;
static int             s_post_accepted;
static SemaphoreHandle_t s_flash_sem;
static TaskHandle_t      s_flash_task;

static const char s_resp_ok[]  = "{\"result\":\"ok\"}";
static const char s_resp_err[] = "{\"result\":\"error\"}";

static inline uint32_t ring_fill(void) { return s_ring_head - s_ring_tail; }
static inline uint32_t ring_free(void) { return UPG_RING_SIZE - ring_fill(); }

/* single-producer / single-consumer, unsigned wrap-around indices */
static uint32_t ring_put(const uint8_t *src, uint32_t len)
{
    uint32_t space = ring_free();
    if (len > space) len = space;
    uint32_t idx = s_ring_head % UPG_RING_SIZE;
    uint32_t first = UPG_RING_SIZE - idx;
    if (first > len) first = len;
    memcpy(&s_ring[idx], src, first);
    if (len > first) memcpy(&s_ring[0], src + first, len - first);
    s_ring_head += len;
    return len;
}

/* Return a contiguous slice starting at the current tail (up to `max`).
 * Caller commits with ring_advance(). */
static uint32_t ring_peek_contig(const uint8_t **ptr, uint32_t max)
{
    uint32_t fill = ring_fill();
    if (fill == 0U) return 0U;
    uint32_t idx = s_ring_tail % UPG_RING_SIZE;
    uint32_t contig = UPG_RING_SIZE - idx;
    uint32_t n = fill;
    if (n > contig) n = contig;
    if (n > max) n = max;
    *ptr = &s_ring[idx];
    return n;
}

static inline void ring_advance(uint32_t n) { s_ring_tail += n; }

static int flash_writer_drain_ring(void)
{
    for (;;) {
        const uint8_t *chunk = NULL;
        uint32_t n = ring_peek_contig(&chunk, 4096U);
        if (n == 0U) return 0;
        if (UpgradeRtosStorage_Append(chunk, n) != 0) {
            printf("[UPG] Append FAILED consumed=%lu\r\n", (unsigned long)s_consumed);
            return -1;
        }
        ring_advance(n);
        s_consumed += n;
    }
}

static void flash_writer_task(void *arg)
{
    (void)arg;
    printf("[FW] started (ring=%u KB)\r\n", (unsigned)(UPG_RING_SIZE / 1024U));
    for (;;) {
        xSemaphoreTake(s_flash_sem, portMAX_DELAY);
        if (s_state != UPG_STATE_RECEIVING && s_state != UPG_STATE_EOF) continue;

        TickType_t t0 = xTaskGetTickCount();
        int rc = 0;

        /* Drain loop: consume the ring, sleep on the sem when empty,
         * exit when producer signals EOF and the ring is drained. */
        while (rc == 0) {
            if (flash_writer_drain_ring() != 0) { rc = -1; break; }
            if (s_state == UPG_STATE_EOF && ring_fill() == 0U) break;
            if (s_state != UPG_STATE_RECEIVING && s_state != UPG_STATE_EOF) {
                rc = -1; break;   /* aborted */
            }
            xSemaphoreTake(s_flash_sem, pdMS_TO_TICKS(100));
        }

        if (rc == 0 && s_consumed != s_expected_size) {
            printf("[FW] size mismatch consumed=%lu expected=%lu\r\n",
                   (unsigned long)s_consumed, (unsigned long)s_expected_size);
            rc = -1;
        }
        if (rc == 0) rc = UpgradeRtosStorage_Finish();

        s_fw_ok = (rc == 0);
        s_state = UPG_STATE_DONE;
        printf("[FW] done rc=%d ms=%lu\r\n", rc, (unsigned long)(xTaskGetTickCount() - t0));
    }
}

err_t httpd_post_begin(void *connection, const char *uri,
                       const char *http_request, u16_t http_request_len,
                       int content_len,
                       char *response_uri, u16_t response_uri_len,
                       u8_t *post_auto_wnd)
{
    (void)http_request; (void)http_request_len;
    (void)connection; (void)response_uri; (void)response_uri_len;

    printf("[HTTP] POST begin uri=%s len=%d\r\n",
           uri ? uri : "null", content_len);

    if (!uri || strcmp(uri, "/api/upgrade") != 0) return ERR_ARG;
    if (content_len <= 0) return ERR_ARG;
    /* refuse if a previous session is still active */
    if (s_state == UPG_STATE_RECEIVING || s_state == UPG_STATE_EOF) return ERR_MEM;

    if (UpgradeRtosStorage_Begin((uint32_t)content_len) != 0) return ERR_MEM;

    s_ring_head     = 0;
    s_ring_tail     = 0;
    s_expected_size = (uint32_t)content_len;
    s_produced      = 0;
    s_consumed      = 0;
    s_fw_ok         = 0;
    s_post_accepted = 0;
    s_state         = UPG_STATE_RECEIVING;

    *post_auto_wnd = 1;  /* lwIP manages the receive window */

    printf("[HTTP] accepted size=%lu ring=%u KB\r\n",
           (unsigned long)s_expected_size,
           (unsigned)(UPG_RING_SIZE / 1024U));
    return ERR_OK;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    (void)connection;
    if (!p) return ERR_OK;

    if (s_state == UPG_STATE_RECEIVING) {
        struct pbuf *q = p;
        uint32_t total_put = 0;
        while (q) {
            uint32_t put = ring_put((const uint8_t *)q->payload, q->len);
            total_put += put;
            if (put != q->len) {
                /* Ring full — should not happen with the sized headroom.
                 * Aborts the transfer; httpd_post_finished will observe the
                 * size mismatch and reject. */
                printf("[RX] ring FULL put=%lu need=%u fill=%lu\r\n",
                       (unsigned long)put, (unsigned)q->len,
                       (unsigned long)ring_fill());
                break;
            }
            q = q->next;
        }
        s_produced += total_put;
        xSemaphoreGive(s_flash_sem);
    }
    pbuf_free(p);
    return ERR_OK;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    (void)connection;
    printf("[HTTP] POST finished prod=%lu/%lu\r\n",
           (unsigned long)s_produced, (unsigned long)s_expected_size);

    if (s_produced != s_expected_size) {
        printf("[HTTP] incomplete POST rejected\r\n");
        s_state = UPG_STATE_DONE;
        s_post_accepted = 0;
        xSemaphoreGive(s_flash_sem);
    } else {
        s_post_accepted = 1;
        s_state = UPG_STATE_EOF;
        xSemaphoreGive(s_flash_sem);
    }
    snprintf(response_uri, response_uri_len, "/upgrade_result.json");
}

int fs_open_custom(struct fs_file *file, const char *name)
{
    if (strcmp(name, "/upgrade_result.json") == 0) {
        const char *body = s_post_accepted ? s_resp_ok : s_resp_err;
        file->data = body; file->len = (int)strlen(body);
        file->index = file->len; file->pextension = NULL; file->flags = 0;
        printf("[FS] → %s\r\n", body);
        return 1;
    }
    if (strncmp(name, "/v1/", 4) == 0 && name[4] != '\0') {
        const uint8_t *data = NULL;
        uint32_t size = 0;
        if (!app_widget_get_embedded_file(name + 4, &data, &size)) return 0;
        memset(file, 0, sizeof(*file));
        file->data = (const char *)data;
        file->len = (int)size;
        file->index = file->len;
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        return 1;
    }
    return 0;
}

void fs_close_custom(struct fs_file *file) { (void)file; }

void RtosHttpServer_Start(void)
{
    if (s_httpd_started) return;
    LOCK_TCPIP_CORE();
    httpd_init();
    UNLOCK_TCPIP_CORE();
    s_httpd_started = 1;
    printf("[HTTP] server started on port %d\r\n", HTTPD_SERVER_PORT);
}

void UpgradeRtosHttp_Start(void)
{
    if (s_upgrade_prepared) {
        RtosHttpServer_Start();
        return;
    }

    printf("[UPG] pre-erasing Flash...\r\n");
    UpgradeRtosStorage_PreErase();

    s_flash_sem = xSemaphoreCreateBinary();
    if (!s_flash_sem) { printf("[UPG] FATAL: sem\r\n"); return; }

    if (xTaskCreate(flash_writer_task, "flash_wr", 1024, NULL,
                    tskIDLE_PRIORITY + 1, &s_flash_task) != pdPASS) {
        printf("[UPG] FATAL: task\r\n"); return;
    }

    s_upgrade_prepared = 1;
    RtosHttpServer_Start();
    printf("[UPG] httpd port=%d TCP_WND=%u(%u MSS) ring=%u KB\r\n",
           HTTPD_SERVER_PORT,
           (unsigned)TCP_WND, (unsigned)(TCP_WND / TCP_MSS),
           (unsigned)(UPG_RING_SIZE / 1024U));
    printf("[UPG] URL: %s\r\n", UpgradeRtosHttp_Url());
}

const char *UpgradeRtosHttp_Url(void)
{
    static char url[64];
    snprintf(url, sizeof(url), "http://%s:%d/api/upgrade",
             ip4addr_ntoa(netif_ip4_addr(&g_mynetif)), HTTPD_SERVER_PORT);
    return url;
}

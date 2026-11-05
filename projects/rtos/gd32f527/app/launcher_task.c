/**
 * @file    launcher_task.c
 * @brief RTOS test firmware launcher.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "netconf.h"

#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client/thing_model_client_session.h"
#include "sample_config.h"
#include "sample_session.h"
#include "sample_lifecycle.h"
#include "upgrade_rtos_http.h"

#include "cJSON.h"

#include <stdio.h>

extern void ThingModelOsal_RegisterFreeRtos(void);

/* ── each sample's core run (shared by both sides, without Connect/Disconnect) ── */
extern int efts_run_once(ThingModelClientSession_t *session);
extern int custom_small_data_run_with_session(ThingModelClientSession_t *session, int argc, char **argv, int debug_tap);
extern int data_subscription_run_with_session(ThingModelClientSession_t *s, int verbose, const char *debug_filter, int observe_sec);
extern int widget_run_with_session(ThingModelClientSession_t *session, int observe_sec);
extern int upgrade_run_with_session(ThingModelClientSession_t *session, int observe_sec);
extern int power_run_with_session(ThingModelClientSession_t *session, int observe_sec);
extern int time_sync_run_with_session(ThingModelClientSession_t *session, int pps_gpio, int observe_sec);

/* ============================================================
 * wait for network ready
 * ============================================================ */
static void wait_network_ready(void)
{
    printf("[launcher] waiting for network ready ...\r\n");
    uint32_t elapsed = 0;
    for (;;) {
        if (netif_is_up(&g_mynetif) &&
            !ip4_addr_isany(netif_ip4_addr(&g_mynetif))) {
            const ip4_addr_t *ip = netif_ip4_addr(&g_mynetif);
            printf("[launcher] network ready: ip=%d.%d.%d.%d (after %lu ms)\r\n",
                   ip4_addr1_16(ip), ip4_addr2_16(ip),
                   ip4_addr3_16(ip), ip4_addr4_16(ip),
                   (unsigned long)elapsed);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        elapsed += 500;
        if (elapsed % 5000 == 0) {
            printf("[launcher] still waiting network (elapsed=%lu ms)\r\n",
                   (unsigned long)elapsed);
        }
    }
}

/* ============================================================
 * Background sample task wrappers — each wrapper takes the session pointer,
 * calls the corresponding _run_with_session, and never returns (sample_should_stop is always false on RTOS).
 * ============================================================ */
static void csd_task(void *arg) {
    custom_small_data_run_with_session((ThingModelClientSession_t *)arg, 0, NULL, 0);
    vTaskDelete(NULL);
}
static void ds_task(void *arg) {
    data_subscription_run_with_session((ThingModelClientSession_t *)arg, 1, NULL, 0);
    vTaskDelete(NULL);
}
static void widget_task(void *arg) {
    widget_run_with_session((ThingModelClientSession_t *)arg, 0);
    vTaskDelete(NULL);
}
static void upgrade_task(void *arg) {
    upgrade_run_with_session((ThingModelClientSession_t *)arg, 0);
    vTaskDelete(NULL);
}
static void power_task(void *arg) {
    power_run_with_session((ThingModelClientSession_t *)arg, 0);
    vTaskDelete(NULL);
}
static void time_sync_task(void *arg) {
    time_sync_run_with_session((ThingModelClientSession_t *)arg, 0, 0);
    vTaskDelete(NULL);
}

/* background task config: (entry, name, stack_words) */
struct bg_task {
    void (*entry)(void *);
    const char *name;
    uint16_t    stack_words;
};
static const struct bg_task g_bg_tasks[] = {
    { csd_task,       "custom_data", 2048 },
    { ds_task,        "data_sub",    4096 },
    { widget_task,    "widget",    2048 },
    { upgrade_task,   "upgrade",   2048 },
    { power_task,     "power",     2048 },
    { time_sync_task, "time_sync", 2048 },
};
#define BG_TASK_COUNT ((int)(sizeof(g_bg_tasks) / sizeof(g_bg_tasks[0])))

/* ============================================================ */
void launcher_task_entry(void *arg)
{
    (void)arg;

    wait_network_ready();

    RtosHttpServer_Start();

    cJSON_Hooks hooks = { .malloc_fn = pvPortMalloc, .free_fn = vPortFree };
    cJSON_InitHooks(&hooks);

    ThingModelOsal_RegisterFreeRtos();
    printf("[launcher] OSAL (FreeRTOS) registered\r\n");

    ThingModelClientSession_Config_t cfg;
    thing_model_sample_config_load(&cfg);
    thing_model_sample_config_print(&cfg);
    sample_lifecycle_install();

    printf("[launcher] sample_session_open ...\r\n");
    ThingModelClientSession_t *s = sample_session_open(&cfg);
    if (!s) {
        printf("[launcher] session connect FAILED (broker=%s)\r\n", cfg.broker_address);
        vTaskDelete(NULL);
    }
    printf("[launcher] session CONNECTED, sn=%s\r\n",
           ThingModelClientSession_GetSn(s) ? ThingModelClientSession_GetSn(s) : "(null)");

    printf("\n[launcher] === EFTS one-shot ===\r\n");
    printf("[launcher] EFTS rc=%d\r\n", efts_run_once(s));

    /* ── background samples: staggered startup, 500ms between each task, to avoid concurrent publishes at startup
     * filling the MQTT output ring buffer (8KB) and causing ERR_MEM. ── */
    printf("\n[launcher] === starting %d background tasks (staggered 500ms) ===\r\n", BG_TASK_COUNT);
    for (int i = 0; i < BG_TASK_COUNT; i++) {
        BaseType_t r = xTaskCreate(g_bg_tasks[i].entry, g_bg_tasks[i].name,
                                   g_bg_tasks[i].stack_words, s,
                                   tskIDLE_PRIORITY + 2, NULL);
        printf("[launcher]   %-10s %s (stack=%u words)\r\n",
               g_bg_tasks[i].name,
               r == pdPASS ? "OK" : "FAIL",
               (unsigned)g_bg_tasks[i].stack_words);
        if (i + 1 < BG_TASK_COUNT)
            vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("[launcher] all background tasks started, launcher task suspending\r\n");
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000));
}

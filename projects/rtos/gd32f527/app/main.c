/**
 * @file    main.c
 * @brief   GD32F527 RTOS sample entry — Things-Model MQTT client.
 *
 * Task model:
 *
 *   main()
 *     ├── xTaskCreate(bsp_init_task)     priority 0, handles BSP/lwIP/RNDIS init + USB TX/RX main loop
 *     ├── xTaskCreate(launcher_task)     priority 0, waits for network ready → ThingModelOsal_RegisterFreeRtos
 *     │                                  → sample_launcher_run() (shared host launcher,
 *     │                                  SAMPLE_AUTORUN="custom_small_data" compile macro injection → run csd directly)
 *     └── (lwIP internal) tcpip_thread    priority tskIDLE+2, created automatically by tcpip_init(),
 *                                        handles ARP/IP/TCP/MQTT timer + inbound packet processing + outbound send
 *
 * lwIP NO_SYS=0 + LWIP_TCPIP_CORE_LOCKING=1 — the application thread can call
 * ThingModelClientSession_Publish from anywhere; transport_lwip automatically does LOCK_TCPIP_CORE then
 * calls mqtt_publish directly. bsp_init_task does **not** periodically run drain / sys_check_timeouts —
 * that work is done by tcpip_thread itself. This task only:
 *   1. Periodically calls USBD_RNDIS_Handle() to drive USB TX/RX (USB is a bare driver)
 *   2. Heartbeat log (idle heap watermark monitoring)
 */
#include "FreeRTOS.h"
#include "task.h"

#include "gd32f5xx.h"
#include "led.h"
#include "uart.h"
#include "usb_device.h"
#include "flash_if.h"

#include "lwip/sys.h"
#include "netconf.h"
#include "usb_rndis_config.h"
#include "thing_model_client/thing_model_client_transport.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ====================================================
 * Task stack sizes
 *
 * !! Note: the third argument of FreeRTOS xTaskCreate is stack depth in **words**
 *    (StackType_t = uint32_t = 4 bytes), not bytes.
 *    2048 words = 8KB stack, sized so bsp_init_task (lwIP+mqtt+mbedTLS) does not overflow —
 *    bsp_init_task runs lwIP+mqtt+mbedTLS, so the stack must be large.
 * ==================================================== */
#define BSP_INIT_TASK_STACK_DEPTH   2048    /* words = 8KB stack */
#define BSP_INIT_TASK_PRIORITY      0
#define LAUNCHER_TASK_STACK_DEPTH   4096    /* words = 16KB stack — mbedTLS/cJSON are stack-hungry */
#define LAUNCHER_TASK_PRIORITY      0

static TaskHandle_t s_bsp_task;
static TaskHandle_t s_launcher_task;

/* UART log mutex init — implemented in the printf retarget section at the bottom of this file, called by main */
void uart_log_lock_init(void);

/* Provided by launcher_task.c — waits for network ready, registers OSAL, then starts the samples. */
extern void launcher_task_entry(void *arg);

/* forward declaration */
static void bsp_init_task(void *arg);

static int cpu_usage_sample(uint32_t *percent_tenths)
{
    static TaskStatus_t tasks[24];
    static uint32_t previous_total;
    static uint32_t previous_idle;
    static int baseline_valid;
    uint32_t total = 0U;
    uint32_t idle = 0U;
    int idle_found = 0;

    /* Keep task names valid while locating Idle in the snapshot. */
    vTaskSuspendAll();
    UBaseType_t count = uxTaskGetSystemState(tasks, sizeof(tasks) / sizeof(tasks[0]), &total);
    for (UBaseType_t i = 0U; i < count; i++) {
        if (strcmp(tasks[i].pcTaskName, "IDLE") == 0) {
            idle = tasks[i].ulRunTimeCounter;
            idle_found = 1;
            break;
        }
    }
    (void)xTaskResumeAll();
    if (!idle_found) {
        baseline_valid = 0;
        return 0;
    }

    uint32_t total_delta = total - previous_total;
    uint32_t idle_delta = idle - previous_idle;
    int wrapped = baseline_valid && total < previous_total;
    int valid = baseline_valid && !wrapped && total_delta != 0U && idle_delta <= total_delta;
    previous_total = total;
    previous_idle = idle;
    /* The bundled kernel skips a switch interval at wrap; rebuild the next baseline. */
    baseline_valid = !wrapped;
    if (valid) {
        *percent_tenths = (uint32_t)(((uint64_t)(total_delta - idle_delta) * 1000U
                                     + total_delta / 2U) / total_delta);
    }
    return valid;
}

/* ============================================================
 * early_putc / early_print: does not depend on printf / newlib, writes UART3 registers directly,
 *   used for the earliest diagnostic output at the main entry.
 *   Assumes the bootloader has already configured the UART3 + GPIO PA0/PA1 clock and mode (the bootloader itself outputs over UART3, confirming it is configured).
 * ============================================================ */
static void early_putc(char c)
{
    usart_data_transmit(UART3, (uint8_t)c);
    while (RESET == usart_flag_get(UART3, USART_FLAG_TBE));
}
static void early_print(const char *s)
{
    while (*s) {
        if (*s == '\n') early_putc('\r');
        early_putc(*s++);
    }
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    /* Earliest diagnostic: depends on no init, writes UART3 putchar directly.
     * Seeing this line = both SystemInit + __libc_init_array ran successfully. */
    early_print("\n[MAIN] app reached main()\n");

    /* NVIC priority grouping 4 — expected by FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    early_print("[MAIN] NVIC priority group set\n");

    /* UART log mutex — created before xTaskCreate, guaranteeing the mutex is ready when the first
     * running task calls printf. The mutex is used by uart3_write_atomic below in this file. */
    uart_log_lock_init();
    early_print("[MAIN] UART log lock initialized\n");

    /* two resident tasks */
    if (xTaskCreate((TaskFunction_t)bsp_init_task, "bsp_init_task",
                BSP_INIT_TASK_STACK_DEPTH,
                NULL, BSP_INIT_TASK_PRIORITY, &s_bsp_task) != pdPASS) {
        early_print("[MAIN] FATAL: xTaskCreate bsp_init_task failed\n");
        for (;;);
    }
    early_print("[MAIN] bsp_init_task created\n");

    if (xTaskCreate((TaskFunction_t)launcher_task_entry, "launcher_task",
                LAUNCHER_TASK_STACK_DEPTH,
                NULL, LAUNCHER_TASK_PRIORITY, &s_launcher_task) != pdPASS) {
        early_print("[MAIN] FATAL: xTaskCreate launcher_task failed\n");
        for (;;);
    }
    early_print("[MAIN] launcher_task created\n");

    early_print("[MAIN] vTaskStartScheduler...\n");
    vTaskStartScheduler();

    /* never reached */
    early_print("[MAIN] FATAL: scheduler returned!\n");
    for (;;);
}

/* ============================================================
 * bsp_init_task
 *
 *   BSP hardware init → wait for USB-RNDIS enumeration → lwIP init (tcpip_init internally starts
 *   tcpip_thread automatically) → DHCP → enter main loop:
 *     - USBD_RNDIS_Handle() drives USB TX/RX (USB is a bare driver, not part of lwIP, must be
 *       driven in task context)
 *     - heartbeat log
 *
 *   Under NO_SYS=0, sys_check_timeouts() and the original transport drain are both done automatically
 *   by the lwIP tcpip_thread; this task no longer needs to call them manually.
 * ============================================================ */
static void bsp_init_task(void *arg)
{
    (void)arg;

    Led_Init(LED_RED);
    Led_Init(LED_GREEN);
    Led_Init(LED_BLUE);
    UART_Init(DJI_CONSOLE_UART_NUM, DJI_CONSOLE_UART_BAUD);
    printf("\r\n======== Payload IoT (Things-Model) firmware start ========\r\n");

    USBD_DEV_Init();
    FLASH_If_Init();
    printf("[BSP] HW init done\r\n");

    /* wait for USB-RNDIS enumeration to complete
     *
     * USBD_RNDIS_IsReady() return value (see bsp/drivers/BSP/usb_device.c):
     *   1 = USB is CONFIGURED (host-side enumeration done, rndis_init() has run,
     *       class_data[USBD_RNDIS_INTERFACE] is bound to rndis_handler)
     *   0 = USB is still in RESET / DEFAULT / ADDRESSED state, not configured
     *
     * We must loop while NOT ready, i.e. while the return value == 0; entering enet_data_to_usb
     * before enumeration completes leaves class_data NULL and faults.
     */
    while (USBD_RNDIS_IsReady() == 0U) {
        for (volatile uint32_t i = 0; i < 200000U; i++) { __NOP(); }
    }
    printf("[BSP] USB-RNDIS ready\r\n");

    /* lwIP init (NO_SYS=0): internal tcpip_init -> starts tcpip_thread -> does netif_add/up
     * in done_cb. When the function returns, tcpip_thread is ready and netif is up. */
    lwip_stack_init();

#ifdef USE_DHCP
    /* DHCP waits for an address — dhcp_addr_status / DHCP_ADDR_* are provided by netconf.h.
     * lwip_dhcp_address_get() already adds LOCK_TCPIP_CORE protection internally. */
    while (dhcp_addr_status != DHCP_ADDR_GOT
           && dhcp_addr_status != DHCP_ADDR_FAIL) {
        USBD_RNDIS_Handle();
        lwip_dhcp_address_get();
    }
    if (dhcp_addr_status == DHCP_ADDR_GOT) {
        printf("[BSP] DHCP IP acquired\r\n");
    } else {
        printf("[BSP] DHCP failed, static fallback\r\n");
    }
#else
    printf("[BSP] Static IP\r\n");
#endif

    Led_On(LED_GREEN);  /* network ready indicator */
    printf("[BSP] Entering USB poll + heartbeat loop\r\n");

    /* main loop — NO_SYS=0:
     *   - lwIP timers run in tcpip_thread; core-locking RX may execute here
     *   - drive USB-RNDIS, sleeping only without RX/control work
     *   - heartbeat log monitors the heap watermark */
    static uint32_t s_heartbeat = 0U;
    for (;;) {
        USBD_RNDIS_Handle();

        /* Tick-based non-Idle estimate and FreeRTOS heap only, not total chip RAM. */
        if (sys_now() - s_heartbeat >= 20000U) {
            s_heartbeat = sys_now();
            uint32_t cpu_tenths = 0U;
            char cpu_est[8] = "n/a";
            if (cpu_usage_sample(&cpu_tenths)) {
                snprintf(cpu_est, sizeof(cpu_est), "%lu.%lu%%",
                         (unsigned long)(cpu_tenths / 10U),
                         (unsigned long)(cpu_tenths % 10U));
            }
            HeapStats_t heap;
            vPortGetHeapStats(&heap);
            printf("[heartbeat] t=%lu free_heap=%lu min=%lu cpu_est=%s "
                   "heap_total=%lu heap_used=%lu heap_peak=%lu heap_largest_free=%lu\r\n",
                   (unsigned long)sys_now(),
                   (unsigned long)heap.xAvailableHeapSpaceInBytes,
                   (unsigned long)heap.xMinimumEverFreeBytesRemaining,
                   cpu_est,
                   (unsigned long)configTOTAL_HEAP_SIZE,
                   (unsigned long)(configTOTAL_HEAP_SIZE - heap.xAvailableHeapSpaceInBytes),
                   (unsigned long)(configTOTAL_HEAP_SIZE - heap.xMinimumEverFreeBytesRemaining),
                   (unsigned long)heap.xSizeOfLargestFreeBlockInBytes);
        }

        /* New work after this snapshot waits until the next tick and scheduling opportunity. */
        if (USBD_RNDIS_HasPendingWork()) {
            taskYIELD();
        } else {
            vTaskDelay(1);
        }
    }
}

/* ============================================================
 * FreeRTOS hooks
 * ============================================================ */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("[FATAL] Stack overflow in task: %s\r\n", pcTaskName);
    for (;;);
}

void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] pvPortMalloc returned NULL — heap exhausted\r\n");
    for (;;);
}

static volatile uint32_t s_idle_count = 0;
void vApplicationIdleHook(void)
{
    s_idle_count++;
}

/* ============================================================
 * Retarget printf → UART3
 *
 * Kept consistent with the expected serial tool configuration.
 *
 * Two levels of fixes:
 *
 * **A. ONLCR (LF→CRLF) done at the hardware layer** —— much of the code the RTOS runs comes from
 * shared host POSIX samples (e.g. samples/sample_things_model/launcher/sample_main.c,
 * samples/sample_things_model/custom_small_data/...), which write `printf("...\n")`
 * without `\r`. A POSIX terminal defaults to ONLCR converting LF to CRLF automatically, but serial tools
 * (minicom/picocom/tio/screen) by default do **not** do this conversion -> a bare `\n` on the serial line
 * only moves the cursor down without returning to the first column, producing a
 * "typewriter staircase" misalignment. When '\n' is seen at the putc layer, emit '\r' first then '\n',
 * fixing all printf callers once and for all, without forcing shared host code to change everything to "\r\n" for the RTOS.
 *
 * **B. Whole-line atomic write (FreeRTOS mutex)** —— bsp_init_task / launcher_task /
 * csd_tx_task / tcpip_thread printing simultaneously causes character-level interleaving. The fix is at the *_write* layer
 * (newlib one printf -> one _write -> one buffer segment), not the __io_putchar layer
 * (a lock per byte cannot guarantee atomicity within a printf). Strategy:
 *   - bsp/.../syscalls.c's _write calls weak bsp_uart_write_atomic(buf, len)
 *   - this file provides the strong override that takes the mutex and writes the whole segment
 *   - leave the __io_putchar/fputc single-char path untouched (no mutex, because their callers such as
 *     fault_handler run in exception context where the mutex is unusable)
 *
 * Exception-context fallback: before the scheduler starts or inside fault_handler (xPortIsInsideInterrupt),
 * if the mutex cannot be taken, go the lock-free path and write UART directly. fault_handler expects best-effort.
 *
 * (Cost: 1 extra byte/newline + one mutex take/give per printf. The blocking polling UART
 *  is the performance bottleneck, so the mutex overhead is negligible.)
 * ============================================================ */
#include "semphr.h"

static SemaphoreHandle_t s_uart_mtx = NULL;   /* lazy-create in main() */

static inline void uart3_putc(uint8_t ch)
{
    usart_data_transmit(UART3, ch);
    while (RESET == usart_flag_get(UART3, USART_FLAG_TBE));
}

/* Single-char ONLCR putc — lock-free version (used by fault_handler / __io_putchar). */
static inline void uart3_putc_onlcr(char c)
{
    if (c == '\n') uart3_putc('\r');
    uart3_putc((uint8_t)c);
}

/* Called once by main() before xTaskCreate, guaranteeing the mutex is ready when the first task runs printf. */
void uart_log_lock_init(void)
{
    if (s_uart_mtx == NULL) s_uart_mtx = xSemaphoreCreateMutex();
}

/* Strong override of the weak bsp_uart_write_atomic in
 * bsp/drivers/CMSIS/GD/GD32F5xx/Source/syscalls.c. newlib `printf` -> `_write(fd, buf, n)` -> this function,
 * the whole buffer is written out byte by byte inside the mutex (incl. ONLCR), guaranteeing whole-line atomicity across tasks. */
int bsp_uart_write_atomic(const char *ptr, int len)
{
    BaseType_t locked = pdFALSE;
    /* Exception context / scheduler not started -> mutex unusable, go lock-free direct write (best-effort). */
    if (s_uart_mtx != NULL && xPortIsInsideInterrupt() == pdFALSE) {
        locked = xSemaphoreTake(s_uart_mtx, portMAX_DELAY);
    }
    for (int i = 0; i < len; i++) {
        uart3_putc_onlcr(ptr[i]);
    }
    if (locked == pdTRUE) xSemaphoreGive(s_uart_mtx);
    return len;
}

/* fputc: newlib stdio compatible — single-char path. Not locked (called infrequently, locking overhead not worth it).
 * Business code using printf (going through batched _write) is automatically locked; direct fputc callers are rare. */
int fputc(int ch, FILE *f)
{
    (void)f;
    uart3_putc_onlcr((char)ch);
    return ch;
}

/* __io_putchar: BSP syscalls.c's default _write would call it (we strong-override
 * _write to go through bsp_uart_write_atomic, so __io_putchar now only has manual callers,
 * e.g. code that directly calls `__io_putchar(c)`. Kept the single-char version, ONLCR still applies. */
int __io_putchar(int ch)
{
    uart3_putc_onlcr((char)ch);
    return 1;
}

int __io_getchar(void)
{
    return 0;
}

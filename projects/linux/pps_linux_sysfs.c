/**
 * @file    pps_linux_sysfs.c
 * @brief   Linux sysfs GPIO PPS capture — implements time_sync_pps.h.
 */
#define _POSIX_C_SOURCE 200809L
#include "time_sync_pps.h"

#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdbool.h>

#define PPS_DEBOUNCE_US 800000ULL

#define TIME_SYNC_PPS_DEFAULT_GPIO 26

static volatile uint64_t s_last_edge_us = 0;
static volatile uint64_t s_prev_edge_us = 0;
static int               s_value_fd     = -1;
static int               s_gpio_pin     = -1;
static T_ThingModelThread s_thread      = { NULL };
static volatile bool     s_running      = false;
static volatile bool     s_inited       = false;
static pps_edge_cb_t     s_cb           = NULL;
static void             *s_cb_user       = NULL;

uint64_t time_sync_pps_now_us(void)       { return ThingModelOsal_TimeUs(); }
uint64_t time_sync_pps_last_edge_us(void) { return s_last_edge_us; }
uint64_t time_sync_pps_prev_edge_us(void) { return s_prev_edge_us; }
bool     time_sync_pps_ready(void)        { return s_inited; }

static bool sysfs_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return false;
    bool ok = (write(fd, val, strlen(val)) == (ssize_t)strlen(val));
    close(fd);
    return ok;
}

static void pps_signal_watcher(void *arg)
{
    (void)arg;
    printf("[PPS] Start watching GPIO %d rising edges...\n", s_gpio_pin);

    while (s_running) {
        struct pollfd pfd = { .fd = s_value_fd, .events = POLLPRI };
        int r = poll(&pfd, 1, 500);
        if (r <= 0) continue;
        if (!(pfd.revents & POLLPRI)) continue;

        char buf;
        lseek(s_value_fd, 0, SEEK_SET);
        (void)read(s_value_fd, &buf, 1);

        uint64_t now_us = ThingModelOsal_TimeUs();

        if (s_last_edge_us > 0 && (now_us - s_last_edge_us) < PPS_DEBOUNCE_US)
            continue;

        s_prev_edge_us = s_last_edge_us;
        s_last_edge_us = now_us;

        if (s_prev_edge_us > 0) {
            uint64_t diff_ms = (now_us - s_prev_edge_us) / 1000;
            printf("[PPS] rising edge, interval %llu ms, local %llu us\n",
                   (unsigned long long)diff_ms, (unsigned long long)now_us);
        } else {
            printf("[PPS] first rising edge, local %llu us\n",
                   (unsigned long long)now_us);
        }

        if (s_cb) s_cb(now_us, s_cb_user);
    }
}

bool time_sync_pps_start(int gpio_pin_hint, pps_edge_cb_t cb, void *user)
{
    if (s_inited) return true;

    s_cb = cb;
    s_cb_user = user;

    char num[16], path[128];
    snprintf(num, sizeof(num), "%d", gpio_pin_hint);

    (void)sysfs_write("/sys/class/gpio/unexport", num);
    if (!sysfs_write("/sys/class/gpio/export", num)) {
        fprintf(stderr, "[PPS] export GPIO %d failed\n", gpio_pin_hint);
        return false;
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_pin_hint);
    if (!sysfs_write(path, "in")) {
        fprintf(stderr, "[PPS] set direction=in failed\n");
        (void)sysfs_write("/sys/class/gpio/unexport", num);
        return false;
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", gpio_pin_hint);
    if (!sysfs_write(path, "rising")) {
        fprintf(stderr, "[PPS] set edge=rising failed\n");
        (void)sysfs_write("/sys/class/gpio/unexport", num);
        return false;
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pin_hint);
    s_value_fd = open(path, O_RDONLY | O_NONBLOCK);
    if (s_value_fd < 0) {
        fprintf(stderr, "[PPS] open value failed\n");
        (void)sysfs_write("/sys/class/gpio/unexport", num);
        return false;
    }

    s_gpio_pin     = gpio_pin_hint;
    s_last_edge_us = 0;
    s_prev_edge_us = 0;
    s_running      = true;

    s_thread = ThingModelOsal_ThreadCreate(pps_signal_watcher, NULL, "pps_watch", 0);
    if (!s_thread._h) {
        fprintf(stderr, "[PPS] failed to create watcher thread\n");
        s_running = false;
        close(s_value_fd);
        s_value_fd = -1;
        (void)sysfs_write("/sys/class/gpio/unexport", num);
        return false;
    }

    s_inited = true;
    printf("[PPS] GPIO %d initialized\n", gpio_pin_hint);
    return true;
}

void time_sync_pps_stop(void)
{
    if (!s_inited) return;

    s_running = false;
    ThingModelOsal_ThreadJoin(s_thread);
    s_thread._h = NULL;

    if (s_value_fd >= 0) {
        close(s_value_fd);
        s_value_fd = -1;
    }
    char num[16];
    snprintf(num, sizeof(num), "%d", s_gpio_pin);
    (void)sysfs_write("/sys/class/gpio/unexport", num);
    s_gpio_pin = -1;
    s_inited = false;
}

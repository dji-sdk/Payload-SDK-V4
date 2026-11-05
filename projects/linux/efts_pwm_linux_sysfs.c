/**
 * @file    efts_pwm_linux_sysfs.c
 * @brief   Linux sysfs PWM output — implements efts_pwm.h.
 */
#define _POSIX_C_SOURCE 200809L
#include "efts_pwm.h"

#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>

#define PWM_PERIOD_NS  (1000000000L / EFTS_PWM_FREQ_HZ)
#define PWM_DUTY_NS    (PWM_PERIOD_NS * EFTS_PWM_DUTY_PCT / 100)

#define EFTS_PWM_DEFAULT_GPIO  18
#define EFTS_PWM_DEFAULT_CHIP   0

static bool   s_inited  = false;
static int    s_chip    = -1;
static int    s_channel = -1;
static int    s_gpio    = -1;
static char   s_chdir[128];

static bool sysfs_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return false;
    bool ok = (write(fd, val, strlen(val)) == (ssize_t)strlen(val));
    close(fd);
    return ok;
}

static bool pwm_write_int(const char *attr, long v)
{
    char path[160], buf[32];
    snprintf(path, sizeof(path), "%s/%s", s_chdir, attr);
    snprintf(buf, sizeof(buf), "%ld", v);
    return sysfs_write(path, buf);
}

static int pin_to_channel(int gpio_pin)
{
    switch (gpio_pin) {
        case 18: case 12: return 0;
        case 19: case 13: return 1;
        default:          return -1;
    }
}

static int resolve_chip(void)
{
    const char *env = getenv("THING_MODEL_EFTS_PWMCHIP");
    if (env && *env) {
        int c = atoi(env);
        if (c >= 0) return c;
    }
    return EFTS_PWM_DEFAULT_CHIP;
}

bool efts_pwm_ready(void) { return s_inited; }

bool efts_pwm_start(int gpio_pin_hint)
{
    if (s_inited) return true;

    int channel = pin_to_channel(gpio_pin_hint);
    if (channel < 0) {
        fprintf(stderr, "[PWM] GPIO %d is not a hardware PWM pin\n", gpio_pin_hint);
        return false;
    }
    int chip = resolve_chip();

    char base[96], num[16], export_path[128], unexport_path[128];
    snprintf(base, sizeof(base), "/sys/class/pwm/pwmchip%d", chip);
    snprintf(s_chdir, sizeof(s_chdir), "%s/pwm%d", base, channel);
    snprintf(num, sizeof(num), "%d", channel);
    snprintf(export_path, sizeof(export_path), "%s/export", base);
    snprintf(unexport_path, sizeof(unexport_path), "%s/unexport", base);

    if (access(s_chdir, F_OK) != 0) {
        (void)sysfs_write(export_path, num);
        for (int i = 0; i < 100 && access(s_chdir, F_OK) != 0; i++) {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 10 * 1000 * 1000 };
            nanosleep(&ts, NULL);
        }
    }
    if (access(s_chdir, F_OK) != 0) {
        fprintf(stderr, "[PWM] export channel %d failed\n", channel);
        return false;
    }

    (void)pwm_write_int("enable", 0);
    if (!pwm_write_int("period", (long)PWM_PERIOD_NS))     goto fail;
    if (!pwm_write_int("duty_cycle", (long)PWM_DUTY_NS))   goto fail;

    char pol_path[160];
    snprintf(pol_path, sizeof(pol_path), "%s/polarity", s_chdir);
    (void)sysfs_write(pol_path, "normal");

    if (!pwm_write_int("enable", 1)) goto fail;

    s_chip = chip;
    s_channel = channel;
    s_gpio = gpio_pin_hint;
    s_inited = true;
    printf("[PWM] hardware PWM started: pwmchip%d/pwm%d (GPIO %d) %dHz / %d%%\n",
           chip, channel, gpio_pin_hint, EFTS_PWM_FREQ_HZ, EFTS_PWM_DUTY_PCT);
    return true;

fail:
    (void)sysfs_write(unexport_path, num);
    fprintf(stderr, "[PWM] configure failed\n");
    return false;
}

void efts_pwm_stop(void)
{
    if (!s_inited) return;

    (void)pwm_write_int("enable", 0);

    char base[96], unexport_path[128], num[16];
    snprintf(base, sizeof(base), "/sys/class/pwm/pwmchip%d", s_chip);
    snprintf(unexport_path, sizeof(unexport_path), "%s/unexport", base);
    snprintf(num, sizeof(num), "%d", s_channel);
    (void)sysfs_write(unexport_path, num);

    s_chip = s_channel = s_gpio = -1;
    s_inited = false;
}

/**
 * @file    efts_pwm.h
 * @brief   EFTS PWM output — domain interface.
 *
 * The EFTS sample sends SelectFtsPwmTrigger over MQTT to enable the
 * payload's PWM trigger port. A real payload outputs a PWM trigger signal
 * on a hardware pin; this module produces that square wave so it can be
 * observed with an oscilloscope.
 */
#ifndef EFTS_PWM_H
#define EFTS_PWM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EFTS_PWM_FREQ_HZ  400
#define EFTS_PWM_DUTY_PCT  50

bool efts_pwm_start(int gpio_pin_hint);
void efts_pwm_stop(void);
bool efts_pwm_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* EFTS_PWM_H */

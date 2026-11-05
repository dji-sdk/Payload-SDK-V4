/**
 * @file    time_sync_pps.h
 * @brief   PPS (Pulse Per Second) signal capture — domain interface.
 *
 * The full time-sync chain fuses two sources:
 *   1. Aircraft pushes UTC wall clock via MQTT /events (method "time_sync").
 *   2. Aircraft outputs 1Hz PPS on a GPIO; payload captures the rising edge
 *      and timestamps it with the local monotonic clock.
 *
 * cb runs in the platform's interrupt/worker context — keep it short.
 */
#ifndef TIME_SYNC_PPS_H
#define TIME_SYNC_PPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pps_edge_cb_t)(uint64_t timestamp_us, void *user);

bool     time_sync_pps_start(int gpio_pin_hint, pps_edge_cb_t cb, void *user);
void     time_sync_pps_stop(void);
bool     time_sync_pps_ready(void);
uint64_t time_sync_pps_last_edge_us(void);
uint64_t time_sync_pps_prev_edge_us(void);
uint64_t time_sync_pps_now_us(void);

#ifdef __cplusplus
}
#endif

#endif /* TIME_SYNC_PPS_H */

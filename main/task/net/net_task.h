/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NET_TASK_H
#define NET_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"
#include "esp_netif_types.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_STATE_WIFI_CONNECTED (1U << 0)
#define NET_STATE_IPV4_READY     (1U << 1)
#define NET_STATE_TIME_SYNCED    (1U << 2)
#define NET_STATE_ALL            (NET_STATE_WIFI_CONNECTED | \
                                  NET_STATE_IPV4_READY | \
                                  NET_STATE_TIME_SYNCED)

typedef enum {
    NET_TASK_STATE_STOPPED = 0,
    NET_TASK_STATE_WAITING_CONFIG,
    NET_TASK_STATE_CONNECTING,
    NET_TASK_STATE_WAITING_IP,
    NET_TASK_STATE_BACKOFF,
    NET_TASK_STATE_ONLINE,
} net_task_state_t;

typedef struct {
    net_task_state_t state;
    uint32_t state_bits;
    esp_netif_ip_info_t ip_info;
    uint8_t disconnect_reason;
    int8_t disconnect_rssi;
    uint32_t reconnect_count;
    uint32_t reconnect_delay_ms;
    time_t last_sync_time;
    char ssid[33];
} net_status_t;

esp_err_t net_task_start(void);
esp_err_t net_task_stop(void);
bool net_task_is_running(void);

esp_err_t net_get_status(net_status_t *status);
esp_err_t net_wait(uint32_t required_states, TickType_t timeout);
esp_err_t net_request_reconnect(void);
esp_err_t net_request_time_sync(void);

esp_err_t net_time_get_epoch(time_t *result);
esp_err_t net_time_get_local(struct tm *result);
esp_err_t net_time_format(char *buffer, size_t size, const char *format);

#ifdef __cplusplus
}
#endif

#endif // NET_TASK_H

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STA_EVENT_STARTED = 0,
    WIFI_STA_EVENT_CONNECTED,
    WIFI_STA_EVENT_DISCONNECTED,
    WIFI_STA_EVENT_GOT_IP,
    WIFI_STA_EVENT_LOST_IP,
} wifi_sta_event_id_t;

typedef struct {
    wifi_sta_event_id_t id;
    union {
        struct {
            uint8_t reason;
            int8_t rssi;
        } disconnected;
        esp_netif_ip_info_t ip_info;
    } data;
} wifi_sta_event_t;

typedef void (*wifi_sta_event_cb_t)(const wifi_sta_event_t *event, void *context);

esp_err_t network_wifi_sta_init(const char *ssid, const char *password,
                                wifi_sta_event_cb_t callback, void *context);
esp_err_t network_wifi_sta_set_credentials(const char *ssid,
                                           const char *password);
esp_err_t network_wifi_sta_connect(void);
esp_err_t network_wifi_sta_disconnect(void);
esp_err_t network_wifi_sta_stop(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_H

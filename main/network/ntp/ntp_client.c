/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ntp_client.h"
#include <stdbool.h>
#include <stdlib.h>
#include "esp_event.h"
#include "esp_netif_sntp.h"

static bool s_initialized;
static ntp_client_sync_cb_t s_sync_callback;
static void *s_sync_context;

static void ntp_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base != NETIF_SNTP_EVENT || event_id != NETIF_SNTP_TIME_SYNC) {
        return;
    }

    const esp_netif_sntp_time_sync_t *sync = event_data;
    if (s_sync_callback) {
        s_sync_callback(sync->tv.tv_sec, s_sync_context);
    }
}

esp_err_t ntp_client_init(const char *server, const char *timezone,
                          ntp_client_sync_cb_t callback, void *context)
{
    if (!server || server[0] == '\0' || !timezone || timezone[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    setenv("TZ", timezone, 1);
    tzset();

    esp_err_t err = esp_event_handler_register(NETIF_SNTP_EVENT,
                                               NETIF_SNTP_TIME_SYNC,
                                               ntp_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(server);
    config.start = false;
    err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        esp_event_handler_unregister(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC,
                                     ntp_event_handler);
        return err;
    }

    s_sync_callback = callback;
    s_sync_context = context;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t ntp_client_start(void)
{
    return s_initialized ? esp_netif_sntp_start() : ESP_ERR_INVALID_STATE;
}

void ntp_client_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    esp_netif_sntp_deinit();
    esp_event_handler_unregister(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC,
                                 ntp_event_handler);
    s_sync_callback = NULL;
    s_sync_context = NULL;
    s_initialized = false;
}

esp_err_t ntp_client_get_epoch(time_t *result)
{
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    *result = time(NULL);
    return ESP_OK;
}

esp_err_t ntp_client_get_local(struct tm *result)
{
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    time_t now = time(NULL);
    return localtime_r(&now, result) ? ESP_OK : ESP_FAIL;
}

esp_err_t ntp_client_format_time(char *buffer, size_t size,
                                 const char *format)
{
    if (!buffer || size == 0 || !format) {
        return ESP_ERR_INVALID_ARG;
    }

    struct tm local_time;
    esp_err_t err = ntp_client_get_local(&local_time);
    if (err != ESP_OK) {
        return err;
    }
    return strftime(buffer, size, format, &local_time) > 0
         ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

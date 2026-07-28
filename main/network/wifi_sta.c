/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifi_sta.h"
#include <stdbool.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "wifi";

static bool s_initialized;
static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_wifi_event_instance;
static esp_event_handler_instance_t s_ip_event_instance;
static wifi_sta_event_cb_t s_event_callback;
static void *s_event_context;

static void wifi_fill_config(wifi_config_t *wifi_config, const char *ssid,
                             const char *password)
{
    memset(wifi_config, 0, sizeof(*wifi_config));
    strlcpy((char *)wifi_config->sta.ssid, ssid,
            sizeof(wifi_config->sta.ssid));
    if (password) {
        strlcpy((char *)wifi_config->sta.password, password,
                sizeof(wifi_config->sta.password));
    }
    wifi_config->sta.threshold.authmode = password && password[0] != '\0'
                                        ? WIFI_AUTH_WPA2_PSK
                                        : WIFI_AUTH_OPEN;
}

static void wifi_notify(const wifi_sta_event_t *event)
{
    if (s_event_callback) {
        s_event_callback(event, s_event_context);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_sta_event_t event = {0};

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            event.id = WIFI_STA_EVENT_STARTED;
            wifi_notify(&event);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            event.id = WIFI_STA_EVENT_CONNECTED;
            wifi_notify(&event);
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *disconnected = event_data;
            event.id = WIFI_STA_EVENT_DISCONNECTED;
            event.data.disconnected.reason = disconnected->reason;
            event.data.disconnected.rssi = disconnected->rssi;
            wifi_notify(&event);
            break;
        }
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            const ip_event_got_ip_t *got_ip = event_data;
            event.id = WIFI_STA_EVENT_GOT_IP;
            event.data.ip_info = got_ip->ip_info;
            wifi_notify(&event);
        } else if (event_id == IP_EVENT_STA_LOST_IP) {
            event.id = WIFI_STA_EVENT_LOST_IP;
            wifi_notify(&event);
        }
    }
}

esp_err_t network_wifi_sta_set_credentials(const char *ssid,
                                           const char *password)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config;
    wifi_fill_config(&wifi_config, ssid, password);
    return esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

esp_err_t network_wifi_sta_init(const char *ssid, const char *password,
                                wifi_sta_event_cb_t callback, void *context)
{
    if (!ssid || ssid[0] == '\0' || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        s_event_callback = callback;
        s_event_context = context;
        return network_wifi_sta_set_credentials(ssid, password);
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        goto fail_netif;
    }

    s_event_callback = callback;
    s_event_context = context;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event_handler, NULL,
                                              &s_wifi_event_instance);
    if (err != ESP_OK) {
        goto fail_wifi;
    }
    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event_handler, NULL,
                                              &s_ip_event_instance);
    if (err != ESP_OK) {
        goto fail_wifi_handler;
    }

    wifi_config_t wifi_config;
    wifi_fill_config(&wifi_config, ssid, password);
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }
    if (err == ESP_OK) {
        s_initialized = true;
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        s_initialized = false;
        goto fail_handlers;
    }

    return ESP_OK;

fail_handlers:
    esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
                                          s_ip_event_instance);
fail_wifi_handler:
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          s_wifi_event_instance);
fail_wifi:
    esp_wifi_deinit();
fail_netif:
    esp_netif_destroy_default_wifi(s_sta_netif);
    s_sta_netif = NULL;
    s_event_callback = NULL;
    s_event_context = NULL;
    ESP_LOGE(TAG, "WiFi initialization failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t network_wifi_sta_connect(void)
{
    return s_initialized ? esp_wifi_connect() : ESP_ERR_INVALID_STATE;
}

esp_err_t network_wifi_sta_disconnect(void)
{
    return s_initialized ? esp_wifi_disconnect() : ESP_ERR_INVALID_STATE;
}

esp_err_t network_wifi_sta_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
                                          s_ip_event_instance);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          s_wifi_event_instance);
    esp_err_t first_error = esp_wifi_stop();
    esp_err_t err = esp_wifi_deinit();
    if (first_error == ESP_OK) {
        first_error = err;
    }
    esp_netif_destroy_default_wifi(s_sta_netif);

    s_sta_netif = NULL;
    s_event_callback = NULL;
    s_event_context = NULL;
    s_initialized = false;
    return first_error;
}

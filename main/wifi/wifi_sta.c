/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifi_sta.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";

static bool s_is_initialized = false;
static bool s_is_connected = false;
static esp_netif_t *s_sta_netif = NULL;

static void wifi_fill_config(wifi_config_t *wifi_config, const char *ssid, const char *password)
{
    memset(wifi_config, 0, sizeof(*wifi_config));
    strncpy((char*)wifi_config->sta.ssid, ssid, sizeof(wifi_config->sta.ssid) - 1);
    wifi_config->sta.ssid[sizeof(wifi_config->sta.ssid) - 1] = '\0';
    if (password) {
        strncpy((char*)wifi_config->sta.password, password, sizeof(wifi_config->sta.password) - 1);
        wifi_config->sta.password[sizeof(wifi_config->sta.password) - 1] = '\0';
    }
    wifi_config->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "成功连接并获取 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        s_is_connected = true;
    }
}

esp_err_t wifi_sta_init(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_is_initialized) {
        wifi_config_t wifi_config;
        wifi_fill_config(&wifi_config, ssid, password);
        esp_wifi_disconnect();
        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set config failed: %s", esp_err_to_name(err));
            return err;
        }
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "connect failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "create STA netif failed");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "driver init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &event_handler, NULL, &instance_any_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi event register failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &event_handler, NULL, &instance_got_ip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ip event register failed: %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t wifi_config;
    wifi_fill_config(&wifi_config, ssid, password);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set STA mode failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        return err;
    }

    s_is_initialized = true;
    return ESP_OK;
}

bool wifi_sta_is_connected(void)
{
    return s_is_connected;
}

esp_err_t wifi_sta_stop(void)
{
    if (s_sta_netif) {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
        s_is_connected = false;
        s_is_initialized = false;
    }
    return ESP_OK;
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"
#include "nvs_driver.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "config";

// 独立的 Key-Value 键名定义
#define NVS_KEY_WIFI_SSID     "wifi_ssid"
#define NVS_KEY_WIFI_PASS     "wifi_pass"
#define NVS_KEY_DEVICE_NAME   "dev_name"
#define NVS_KEY_DEVICE_ID     "dev_id"

static sys_config_t s_current_config;
static bool s_is_initialized = false;

// 默认出厂配置
static const sys_config_t s_default_config = {
    .wifi_ssid = "",
    .wifi_password = "",
    .device_name = "AC-Remote",
    .device_id = 1001
};

static void log_default_write_result(const char *key, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write default %s failed: %s", key, esp_err_to_name(err));
    }
}

static esp_err_t keep_first_error(esp_err_t current, esp_err_t next)
{
    return (current == ESP_OK) ? next : current;
}

esp_err_t sys_config_init(void)
{
    if (s_is_initialized) {
        return ESP_OK;
    }

    // 1. 初始化存储驱动 (目标分区: storage)
    esp_err_t err = nvs_driver_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化 storage NVS 驱动失败");
        return err;
    }

    // 2. 逐个 Key 读取配置，若不存在则填入默认值并写入 storage
    // (1) WiFi SSID
    err = nvs_driver_read_string(NVS_KEY_WIFI_SSID, s_current_config.wifi_ssid, sizeof(s_current_config.wifi_ssid));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_WIFI_SSID, esp_err_to_name(err));
        strncpy(s_current_config.wifi_ssid, s_default_config.wifi_ssid, sizeof(s_current_config.wifi_ssid) - 1);
        s_current_config.wifi_ssid[sizeof(s_current_config.wifi_ssid) - 1] = '\0';
        log_default_write_result(NVS_KEY_WIFI_SSID,
                                 nvs_driver_write_string(NVS_KEY_WIFI_SSID, s_current_config.wifi_ssid));
    }

    // (2) WiFi 密码
    err = nvs_driver_read_string(NVS_KEY_WIFI_PASS, s_current_config.wifi_password, sizeof(s_current_config.wifi_password));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_WIFI_PASS, esp_err_to_name(err));
        strncpy(s_current_config.wifi_password, s_default_config.wifi_password, sizeof(s_current_config.wifi_password) - 1);
        s_current_config.wifi_password[sizeof(s_current_config.wifi_password) - 1] = '\0';
        log_default_write_result(NVS_KEY_WIFI_PASS,
                                 nvs_driver_write_string(NVS_KEY_WIFI_PASS, s_current_config.wifi_password));
    }

    // (3) 设备名称
    err = nvs_driver_read_string(NVS_KEY_DEVICE_NAME, s_current_config.device_name, sizeof(s_current_config.device_name));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_DEVICE_NAME, esp_err_to_name(err));
        strncpy(s_current_config.device_name, s_default_config.device_name, sizeof(s_current_config.device_name) - 1);
        s_current_config.device_name[sizeof(s_current_config.device_name) - 1] = '\0';
        log_default_write_result(NVS_KEY_DEVICE_NAME,
                                 nvs_driver_write_string(NVS_KEY_DEVICE_NAME, s_current_config.device_name));
    }

    // (4) 设备 ID
    err = nvs_driver_read_u16(NVS_KEY_DEVICE_ID, &s_current_config.device_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_DEVICE_ID, esp_err_to_name(err));
        s_current_config.device_id = s_default_config.device_id;
        log_default_write_result(NVS_KEY_DEVICE_ID,
                                 nvs_driver_write_u16(NVS_KEY_DEVICE_ID, s_current_config.device_id));
    }

    s_is_initialized = true;
    return ESP_OK;
}

esp_err_t sys_config_get(sys_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_is_initialized) {
        esp_err_t err = sys_config_init();
        if (err != ESP_OK) {
            return err;
        }
    }

    memcpy(config, &s_current_config, sizeof(sys_config_t));
    return ESP_OK;
}

esp_err_t sys_config_save(const sys_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    err = keep_first_error(err, nvs_driver_write_string(NVS_KEY_WIFI_SSID, config->wifi_ssid));
    err = keep_first_error(err, nvs_driver_write_string(NVS_KEY_WIFI_PASS, config->wifi_password));
    err = keep_first_error(err, nvs_driver_write_string(NVS_KEY_DEVICE_NAME, config->device_name));
    err = keep_first_error(err, nvs_driver_write_u16(NVS_KEY_DEVICE_ID, config->device_id));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save config failed: %s", esp_err_to_name(err));
        return err;
    }

    memcpy(&s_current_config, config, sizeof(sys_config_t));
    return ESP_OK;
}

esp_err_t sys_config_reset_factory(void)
{
    return sys_config_save(&s_default_config);
}

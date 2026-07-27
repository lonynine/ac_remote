/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "net_task.h"
#include "config.h"
#include "wifi_sta.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "net_task";
static TaskHandle_t s_net_task_handle = NULL;

static void net_task_entry(void *pvParameters)
{
    while (1) {
        sys_config_t cfg;
        if (sys_config_get(&cfg) == ESP_OK && strlen(cfg.wifi_ssid) > 0) {

            if (!wifi_sta_is_connected()) {
                wifi_sta_init(cfg.wifi_ssid, cfg.wifi_password);

                for (int i = 0; i < 10; i++) {
                    if (wifi_sta_is_connected()) {
                        ESP_LOGI(TAG, "WiFi [%s] 已成功连接并获取 IP", cfg.wifi_ssid);
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }

                if (!wifi_sta_is_connected()) {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }

    s_net_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t net_task_start(void)
{
    if (s_net_task_handle != NULL) {
        return ESP_OK; // 已经在运行中
    }

    BaseType_t ret = xTaskCreate(net_task_entry, "net_task", 4096, NULL, 5, &s_net_task_handle);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

esp_err_t net_task_stop(void)
{
    if (s_net_task_handle != NULL) {
        vTaskDelete(s_net_task_handle);
        s_net_task_handle = NULL;
        wifi_sta_stop();
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

bool net_task_is_running(void)
{
    return (s_net_task_handle != NULL);
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor_task.h"
#include "aht20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sensor";

static TaskHandle_t s_sensor_task_handle = NULL;
static SemaphoreHandle_t s_sensor_mutex = NULL;

static sensor_data_t s_latest_data = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .valid = false,
    .updated_at_us = 0,
};

static void sensor_set_valid(bool valid)
{
    if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_latest_data.valid = valid;
        xSemaphoreGive(s_sensor_mutex);
    }
}

static void sensor_wait_ready(void)
{
    esp_err_t err;
    do {
        err = aht20_init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "aht20 init failed: %s, retry later", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    } while (err != ESP_OK);
}

static void sensor_task_proc(void *pvParameters)
{
    sensor_wait_ready();

    float temp = 0.0f;
    float humi = 0.0f;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        esp_err_t err = aht20_read_data(&temp, &humi);
        if (err == ESP_OK) {
            if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_latest_data.temperature = temp;
                s_latest_data.humidity = humi;
                s_latest_data.valid = true;
                s_latest_data.updated_at_us = esp_timer_get_time();
                xSemaphoreGive(s_sensor_mutex);
            }
            ESP_LOGI(TAG, "env temp=%.1fC humi=%.1f%%", temp, humi);
        } else {
            ESP_LOGW(TAG, "aht20 read failed: %s", esp_err_to_name(err));
            sensor_set_valid(false);
            sensor_wait_ready();
            last_wake_time = xTaskGetTickCount();
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
    }
}

esp_err_t sensor_task_init(void)
{
    if (s_sensor_mutex == NULL) {
        s_sensor_mutex = xSemaphoreCreateMutex();
        if (s_sensor_mutex == NULL) {
            ESP_LOGE(TAG, "创建传感器互斥锁失败!");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t sensor_task_start(void)
{
    if (s_sensor_task_handle != NULL) {
        return ESP_OK; // 已启动
    }

    if (s_sensor_mutex == NULL) {
        esp_err_t err = sensor_task_init();
        if (err != ESP_OK) return err;
    }

    BaseType_t ret = xTaskCreate(sensor_task_proc, "sensor_task", 3072, NULL, 4, &s_sensor_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 sensor_task 任务失败!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sensor_task_stop(void)
{
    if (s_sensor_task_handle != NULL) {
        vTaskDelete(s_sensor_task_handle);
        s_sensor_task_handle = NULL;
        aht20_deinit();
        sensor_set_valid(false);
        ESP_LOGI(TAG, "温湿度采集任务已停止");
    }
    return ESP_OK;
}

bool sensor_task_is_running(void)
{
    return s_sensor_task_handle != NULL;
}

esp_err_t sensor_task_get_data(float *temp, float *humi)
{
    sensor_data_t data;
    esp_err_t err = sensor_task_get_status(&data);
    if (err != ESP_OK) {
        return err;
    }
    if (temp) *temp = data.temperature;
    if (humi) *humi = data.humidity;
    return ESP_OK;
}

esp_err_t sensor_task_get_status(sensor_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_sensor_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_latest_data.valid) {
            *data = s_latest_data;
            err = ESP_OK;
        } else {
            err = ESP_ERR_INVALID_STATE;
        }
        xSemaphoreGive(s_sensor_mutex);
    }
    return err;
}

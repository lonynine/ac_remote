/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aht20.h"
#include "i2c_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "aht20";
#define AHT20_I2C_ADDR 0x38

esp_err_t aht20_init(void)
{
    esp_err_t err = i2c_driver_init();
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100)); // 上电延迟

    // 读取状态标志，若未初始化则发送初始化指令 0xBE, 0x08, 0x00
    uint8_t status = 0;
    err = i2c_driver_read(AHT20_I2C_ADDR, &status, 1);
    if (err != ESP_OK || (status & 0x08) == 0) {
        uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
        err = i2c_driver_write(AHT20_I2C_ADDR, init_cmd, 3);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "init command failed: %s", esp_err_to_name(err));
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "ready addr=0x38");
    return ESP_OK;
}

esp_err_t aht20_deinit(void)
{
    esp_err_t err = i2c_driver_remove_device(AHT20_I2C_ADDR);
    return (err == ESP_ERR_NOT_FOUND) ? ESP_OK : err;
}

esp_err_t aht20_read_data(float *out_temp, float *out_humi)
{
    if (!out_temp || !out_humi) return ESP_ERR_INVALID_ARG;

    // 1. 发起触发测量指令 0xAC, 0x33, 0x00
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00};
    esp_err_t err = i2c_driver_write(AHT20_I2C_ADDR, trigger_cmd, 3);
    if (err != ESP_OK) return err;

    // 2. 等待 80ms 测量完成
    vTaskDelay(pdMS_TO_TICKS(80));

    // 3. 读取 6 字节数据
    uint8_t data[6] = {0};
    err = i2c_driver_read(AHT20_I2C_ADDR, data, 6);
    if (err != ESP_OK) return err;

    // 4. 解析湿度数据 (20 位)
    uint32_t raw_humi = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((data[3] & 0xF0) >> 4);
    *out_humi = ((float)raw_humi * 100.0f) / 1048576.0f;

    // 5. 解析温度数据 (20 位)
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    *out_temp = (((float)raw_temp * 200.0f) / 1048576.0f) - 50.0f;

    return ESP_OK;
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2c_driver.h"
#include <stdbool.h>
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_CACHED_DEVICE_MAX 4

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus_handle = NULL;

typedef struct {
    bool used;
    uint8_t dev_addr;
    i2c_master_dev_handle_t handle;
} i2c_cached_device_t;

static i2c_cached_device_t s_cached_devices[I2C_CACHED_DEVICE_MAX];

static esp_err_t i2c_driver_get_device(uint8_t dev_addr, i2c_master_dev_handle_t *out_handle)
{
    if (!s_bus_handle || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < I2C_CACHED_DEVICE_MAX; i++) {
        if (s_cached_devices[i].used && s_cached_devices[i].dev_addr == dev_addr) {
            *out_handle = s_cached_devices[i].handle;
            return ESP_OK;
        }
    }

    for (size_t i = 0; i < I2C_CACHED_DEVICE_MAX; i++) {
        if (!s_cached_devices[i].used) {
            i2c_device_config_t dev_config = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = dev_addr,
                .scl_speed_hz = I2C_MASTER_FREQ_HZ,
            };

            esp_err_t err = i2c_master_bus_add_device(s_bus_handle, &dev_config,
                                                      &s_cached_devices[i].handle);
            if (err != ESP_OK) {
                return err;
            }

            s_cached_devices[i].used = true;
            s_cached_devices[i].dev_addr = dev_addr;
            *out_handle = s_cached_devices[i].handle;
            ESP_LOGI(TAG, "device cached addr=0x%02X", dev_addr);
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

esp_err_t i2c_driver_init(void)
{
    if (s_bus_handle != NULL) {
        return ESP_OK; // 已经初始化
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "bus init sda=%d scl=%d freq=%dHz",
                 I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    } else {
        ESP_LOGE(TAG, "I2C 总线初始化失败: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t i2c_driver_write(uint8_t dev_addr, const uint8_t *data, size_t len)
{
    if (!s_bus_handle || !data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_driver_get_device(dev_addr, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_transmit(dev_handle, data, len, -1);
    return err;
}

esp_err_t i2c_driver_read(uint8_t dev_addr, uint8_t *out_data, size_t len)
{
    if (!s_bus_handle || !out_data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_driver_get_device(dev_addr, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_receive(dev_handle, out_data, len, -1);
    return err;
}

esp_err_t i2c_driver_write_read(uint8_t dev_addr, const uint8_t *write_buf, size_t write_len, uint8_t *read_buf, size_t read_len)
{
    if (!s_bus_handle || !write_buf || !read_buf) return ESP_ERR_INVALID_ARG;

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_driver_get_device(dev_addr, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_transmit_receive(dev_handle, write_buf, write_len, read_buf, read_len, -1);
    return err;
}

esp_err_t i2c_driver_remove_device(uint8_t dev_addr)
{
    for (size_t i = 0; i < I2C_CACHED_DEVICE_MAX; i++) {
        if (s_cached_devices[i].used && s_cached_devices[i].dev_addr == dev_addr) {
            esp_err_t err = i2c_master_bus_rm_device(s_cached_devices[i].handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "remove device 0x%02X failed: %s", dev_addr, esp_err_to_name(err));
                return err;
            }

            s_cached_devices[i].used = false;
            s_cached_devices[i].dev_addr = 0;
            s_cached_devices[i].handle = NULL;
            ESP_LOGI(TAG, "device removed addr=0x%02X", dev_addr);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

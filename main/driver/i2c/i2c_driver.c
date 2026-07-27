/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2c_driver.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "i2c_driver";
static i2c_master_bus_handle_t s_bus_handle = NULL;

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
        ESP_LOGI(TAG, "I2C 总线底层驱动初始化成功! (SDA: IO%d, SCL: IO%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    } else {
        ESP_LOGE(TAG, "I2C 总线初始化失败: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t i2c_driver_write(uint8_t dev_addr, const uint8_t *data, size_t len)
{
    if (!s_bus_handle || !data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(s_bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_transmit(dev_handle, data, len, -1);
    i2c_master_bus_rm_device(dev_handle);
    return err;
}

esp_err_t i2c_driver_read(uint8_t dev_addr, uint8_t *out_data, size_t len)
{
    if (!s_bus_handle || !out_data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(s_bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_receive(dev_handle, out_data, len, -1);
    i2c_master_bus_rm_device(dev_handle);
    return err;
}

esp_err_t i2c_driver_write_read(uint8_t dev_addr, const uint8_t *write_buf, size_t write_len, uint8_t *read_buf, size_t read_len)
{
    if (!s_bus_handle || !write_buf || !read_buf) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev_handle;
    esp_err_t err = i2c_master_bus_add_device(s_bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) return err;

    err = i2c_master_transmit_receive(dev_handle, write_buf, write_len, read_buf, read_len, -1);
    i2c_master_bus_rm_device(dev_handle);
    return err;
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_MASTER_SCL_IO      GPIO_NUM_47
#define I2C_MASTER_SDA_IO      GPIO_NUM_48
#define I2C_MASTER_FREQ_HZ     100000

/**
 * @brief 初始化底层 I2C 主机总线驱动 (SDA: IO6, SCL: IO7)
 */
esp_err_t i2c_driver_init(void);

/**
 * @brief 向 I2C 从机写数据
 */
esp_err_t i2c_driver_write(uint8_t dev_addr, const uint8_t *data, size_t len);

/**
 * @brief 从 I2C 从机读数据
 */
esp_err_t i2c_driver_read(uint8_t dev_addr, uint8_t *out_data, size_t len);

/**
 * @brief 先写寄存器指令，再从 I2C 从机读取数据
 */
esp_err_t i2c_driver_write_read(uint8_t dev_addr, const uint8_t *write_buf, size_t write_len, uint8_t *read_buf, size_t read_len);

/**
 * @brief 从缓存中移除指定 I2C 从设备
 */
esp_err_t i2c_driver_remove_device(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif // I2C_DRIVER_H

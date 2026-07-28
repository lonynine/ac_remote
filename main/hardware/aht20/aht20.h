/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AHT20_H
#define AHT20_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AHT20 温湿度传感器 (包含底层 I2C 初始化与传感器校准)
 */
esp_err_t aht20_init(void);

/**
 * @brief 释放 AHT20 传感器 I2C 设备句柄
 */
esp_err_t aht20_deinit(void);

/**
 * @brief 读取 AHT20 传感器当前的实时温度与相对湿度
 *
 * @param out_temp 温度输出 (单位: ℃)
 * @param out_humi 相对湿度输出 (单位: %)
 */
esp_err_t aht20_read_data(float *out_temp, float *out_humi);

#ifdef __cplusplus
}
#endif

#endif // AHT20_H

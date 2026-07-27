/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NVS_DRIVER_H
#define NVS_DRIVER_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 storage 分区的 NVS Flash 存储
 */
esp_err_t nvs_driver_init(void);

/**
 * @brief 读取字符串类型的 Key-Value
 */
esp_err_t nvs_driver_read_string(const char *key, char *out_str, size_t max_len);

/**
 * @brief 写入字符串类型的 Key-Value
 */
esp_err_t nvs_driver_write_string(const char *key, const char *str);

/**
 * @brief 读取 uint16 类型的 Key-Value
 */
esp_err_t nvs_driver_read_u16(const char *key, uint16_t *out_val);

/**
 * @brief 写入 uint16 类型的 Key-Value
 */
esp_err_t nvs_driver_write_u16(const char *key, uint16_t val);

/**
 * @brief 读取 uint8 类型的 Key-Value
 */
esp_err_t nvs_driver_read_u8(const char *key, uint8_t *out_val);

/**
 * @brief 写入 uint8 类型的 Key-Value
 */
esp_err_t nvs_driver_write_u8(const char *key, uint8_t val);

/**
 * @brief 擦除指定 Key 键值
 */
esp_err_t nvs_driver_erase_key(const char *key);

#ifdef __cplusplus
}
#endif

#endif // NVS_DRIVER_H

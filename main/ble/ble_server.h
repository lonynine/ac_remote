/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 BLE 调参服务并启动广播
 *
 * @param device_name BLE 广播的设备名称
 * @return esp_err_t ESP_OK 表示启动成功
 */
esp_err_t ble_server_init(const char *device_name);

/**
 * @brief 检查 BLE 是否有手机 App 客户端已连接
 *
 * @return true 有手机已连接
 * @return false 无连接
 */
bool ble_server_is_connected(void);

/**
 * @brief 停止 BLE 蓝牙服务
 *
 * @return esp_err_t ESP_OK
 */
esp_err_t ble_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_SERVER_H

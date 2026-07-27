/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 直接指定 SSID 与密码初始化连接 WiFi STA
 */
esp_err_t wifi_sta_init(const char *ssid, const char *password);

/**
 * @brief 检查 WiFi STA 是否已成功连接并获取到 IP 地址
 */
bool wifi_sta_is_connected(void);

/**
 * @brief 停止 WiFi STA 模块
 */
esp_err_t wifi_sta_stop(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_H

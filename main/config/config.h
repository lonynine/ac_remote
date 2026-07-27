/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_WIFI_SSID_LEN     32
#define CONFIG_WIFI_PASS_LEN     64
#define CONFIG_DEVICE_NAME_LEN   32

typedef struct {
    char wifi_ssid[CONFIG_WIFI_SSID_LEN];
    char wifi_password[CONFIG_WIFI_PASS_LEN];
    char device_name[CONFIG_DEVICE_NAME_LEN];
    uint16_t device_id;
} sys_config_t;

/**
 * @brief 初始化应用配置模块（内部自动调用 nvs_driver 初始化并载入参数）
 */
esp_err_t sys_config_init(void);

/**
 * @brief 获取当前系统的配置参数
 */
esp_err_t sys_config_get(sys_config_t *config);

/**
 * @brief 保存更新后的系统配置参数至 NVS 存储
 */
esp_err_t sys_config_save(const sys_config_t *config);

/**
 * @brief 恢复系统出厂默认配置
 */
esp_err_t sys_config_reset_factory(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H

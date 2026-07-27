/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CMD_AC_H
#define CMD_AC_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 'ac' 控制台命令 (支持 ac get, ac set)
 */
esp_err_t register_cmd_ac(void);

#ifdef __cplusplus
}
#endif

#endif // CMD_AC_H

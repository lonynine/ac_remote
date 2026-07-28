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
 * @brief 注册空调控制 CLI 命令行指令 ('ac send', 'ac timer', 'ac learn', 'ac emit' 等)
 */
esp_err_t register_cmd_ac(void);

#ifdef __cplusplus
}
#endif

#endif // CMD_AC_H

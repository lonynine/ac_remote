/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CMD_TASK_H
#define CMD_TASK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 'task' 控制台命令 (支持 task start, task stop, task status)
 */
esp_err_t register_cmd_task(void);

#ifdef __cplusplus
}
#endif

#endif // CMD_TASK_H

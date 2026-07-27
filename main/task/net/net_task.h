/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NET_TASK_H
#define NET_TASK_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建并启动独立的网络管理任务 (net_task)
 */
esp_err_t net_task_start(void);

/**
 * @brief 停止/挂起网络管理任务 (net_task) 并安全断开网络
 */
esp_err_t net_task_stop(void);

/**
 * @brief 检查网络任务当前是否正在运行
 */
bool net_task_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // NET_TASK_H

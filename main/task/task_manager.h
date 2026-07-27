/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*task_start_fn)(void);
typedef esp_err_t (*task_stop_fn)(void);
typedef bool (*task_is_running_fn)(void);

typedef struct {
    const char *name;              // 任务唯一标识 (如: "net")
    const char *description;       // 任务功能描述
    task_start_fn start;           // 启动函数指针
    task_stop_fn stop;             // 停止函数指针
    task_is_running_fn is_running; // 状态查询函数指针
} task_item_t;

/**
 * @brief 初始化任务管理中间层
 */
esp_err_t task_manager_init(void);

/**
 * @brief 根据任务标识统一启动某个任务
 */
esp_err_t task_manager_start(const char *name);

/**
 * @brief 根据任务标识统一停止某个任务
 */
esp_err_t task_manager_stop(const char *name);

/**
 * @brief 检查指定任务标识的任务当前是否正在运行
 */
bool task_manager_is_running(const char *name);

/**
 * @brief 格式化打印当前系统已注册的所有任务及运行状态
 */
void task_manager_print_all_status(void);

#ifdef __cplusplus
}
#endif

#endif // TASK_MANAGER_H

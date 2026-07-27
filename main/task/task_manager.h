/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 统一任务登记项结构体
typedef struct {
    const char *name;
    const char *description;
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    bool (*is_running)(void);
} task_item_t;

/**
 * @brief 初始化任务管理中间层
 */
esp_err_t task_manager_init(void);

/**
 * @brief 根据任务名称启动后台任务
 */
esp_err_t task_manager_start(const char *name);

/**
 * @brief 根据任务名称停止后台任务
 */
esp_err_t task_manager_stop(const char *name);

/**
 * @brief 查询指定任务是否在运行
 */
bool task_manager_is_running(const char *name);

/**
 * @brief 打印所有注册后台任务的状态列表
 */
void task_manager_print_status(void);
void task_manager_print_all_status(void);

#ifdef __cplusplus
}
#endif

#endif // TASK_MANAGER_H

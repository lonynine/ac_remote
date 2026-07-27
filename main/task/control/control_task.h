/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "esp_err.h"
#include "ir_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// 控制消息类型
typedef enum {
    CONTROL_MSG_TYPE_CMD = 0,   // 标准空调控制指令 (ac_remote_cmd_t)
    CONTROL_MSG_TYPE_EMIT = 1,  // 重发上一次学到的红外波形
} control_msg_type_t;

// 统一控制消息结构体
typedef struct {
    control_msg_type_t type;
    ac_remote_cmd_t cmd;
} control_msg_t;

/**
 * @brief 初始化控制任务与 FreeRTOS 独占队列
 */
esp_err_t control_task_init(void);

/**
 * @brief 启动控制任务
 */
esp_err_t control_task_start(void);

/**
 * @brief 停止控制任务
 */
esp_err_t control_task_stop(void);

/**
 * @brief 查询控制任务运行状态
 */
bool control_task_is_running(void);

/**
 * @brief 唯一安全发波入口：向控制队列发送指令 (硬件资源独占保护)
 */
esp_err_t control_task_post_cmd(const ac_remote_cmd_t *cmd);

/**
 * @brief 唯一安全发波入口：向控制队列发送重发学码指令
 */
esp_err_t control_task_post_emit(void);

#ifdef __cplusplus
}
#endif

#endif // CONTROL_TASK_H

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AC_STATE_H
#define AC_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ir_remote.h"

#ifdef __cplusplus
extern "C" {
#endif

// 空调全量状态结构体 (包含红外控制参数 + 扩展定时状态)
typedef struct {
    bool power;            // 开关状态 (true:开机, false:关机)
    gree_mode_t mode;      // 运行模式 (COOL, HEAT, AUTO, DRY, FAN)
    uint8_t temp;          // 目标温度 (16 ~ 30 ℃)
    gree_fan_t fan;        // 风速 (AUTO, LOW, MED, HIGH)
    bool swing;            // 扫风 (true:开启, false:关闭)
    bool light;            // 显示屏灯 (true:开启, false:关闭)
    uint16_t timer_min;    // 定时器剩余时间 (单位: 分钟，0 表示无定时)
} ac_state_t;

/**
 * @brief 初始化空调状态管理模块 (从 NVS 自动读取并恢复上一次保存的状态)
 */
esp_err_t ac_state_init(void);

/**
 * @brief 获取当前最新的空调状态
 */
esp_err_t ac_state_get(ac_state_t *state);

/**
 * @brief 设置并更新空调状态 (自动写入 NVS 持久化)
 */
esp_err_t ac_state_set(const ac_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // AC_STATE_H

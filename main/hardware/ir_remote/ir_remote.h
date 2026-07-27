/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "ir_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_TX_GPIO_NUM  GPIO_NUM_4  // 红外发送引脚: IO4
#define IR_RX_GPIO_NUM  GPIO_NUM_5  // 红外接收引脚: IO5

// 兼容别名
typedef ac_mode_t gree_mode_t;
typedef ac_fan_t  gree_fan_t;
typedef ac_mode_t haier_mode_t;
typedef ac_fan_t  haier_fan_t;

#define GREE_MODE_AUTO AC_MODE_AUTO
#define GREE_MODE_COOL AC_MODE_COOL
#define GREE_MODE_DRY  AC_MODE_DRY
#define GREE_MODE_FAN  AC_MODE_FAN
#define GREE_MODE_HEAT AC_MODE_HEAT

#define GREE_FAN_AUTO  AC_FAN_AUTO
#define GREE_FAN_LOW   AC_FAN_LOW
#define GREE_FAN_MED   AC_FAN_MED
#define GREE_FAN_HIGH  AC_FAN_HIGH

typedef ac_remote_cmd_t haier_ac_status_t;
typedef ac_remote_cmd_t gree_ac_status_t;

/**
 * @brief 初始化红外遥控硬件设备 (TX: IO4, RX: IO5)
 */
esp_err_t ir_remote_init(void);

/**
 * @brief 统一纯 C 语言红外遥控数据包发送接口 (基于 main/protocol/ 纯 C 协议层)
 */
esp_err_t ir_remote_send_cmd(const ac_remote_cmd_t *cmd);

/**
 * @brief 兼容接口：发送海尔/格力空调数据包
 */
esp_err_t ir_remote_send_haier(const haier_ac_status_t *status);
esp_err_t ir_remote_send_gree(const gree_ac_status_t *status);

/**
 * @brief 开启红外学码功能
 */
esp_err_t ir_remote_learn_start(uint32_t timeout_sec);

/**
 * @brief 重发上一次成功学到的红外原始波形数据包
 */
esp_err_t ir_remote_learn_emit(void);

#ifdef __cplusplus
}
#endif

#endif // IR_REMOTE_H

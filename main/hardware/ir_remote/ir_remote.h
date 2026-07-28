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
#include "protocol_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_TX_GPIO_NUM  GPIO_NUM_4  // 红外发送引脚: IO4
#define IR_RX_GPIO_NUM  GPIO_NUM_5  // 红外接收引脚: IO5

/**
 * @brief 初始化红外遥控硬件设备 (TX: IO4, RX: IO5)
 */
esp_err_t ir_remote_init(void);

/**
 * @brief 发送协议层已经编码完成的红外帧
 */
esp_err_t ir_remote_send_frame(const ir_frame_t *frame);

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

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IR_DRIVER_H
#define IR_DRIVER_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/rmt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化底层 RMT 脉冲发送总线 (38kHz 载波调制)
 */
esp_err_t ir_driver_tx_init(gpio_num_t gpio_num, uint32_t carrier_freq_hz);

/**
 * @brief 底层发波：直接向 RMT 发送原始脉冲符号数组
 */
esp_err_t ir_driver_tx_symbols(const rmt_symbol_word_t *symbols, size_t count);

/**
 * @brief 初始化底层 RMT 脉冲接收总线
 */
esp_err_t ir_driver_rx_init(gpio_num_t gpio_num);

/**
 * @brief 底层接收：阻塞接收遥控器发出的红外原始脉冲符号数据
 */
esp_err_t ir_driver_rx_receive(rmt_symbol_word_t *symbols_buf, size_t buf_capacity, size_t *out_count, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // IR_DRIVER_H

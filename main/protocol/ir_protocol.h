/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IR_PROTOCOL_H
#define IR_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/rmt_types.h"
#include "ac_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 脉冲生成公共工具函数
size_t ir_protocol_append_byte(rmt_symbol_word_t *symbols, size_t index, uint8_t data, 
                                uint32_t mark_us, uint32_t space0_us, uint32_t space1_us);

// 16 进制 Hex 原始帧格式化打印工具
void ir_protocol_print_hex(const char *brand_name, const uint8_t *bytes, size_t len);

#ifdef __cplusplus
}
#endif

#endif // IR_PROTOCOL_H

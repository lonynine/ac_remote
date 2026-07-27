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

#ifdef __cplusplus
extern "C" {
#endif

// 空调品牌枚举 (Supported AC Brands)
typedef enum {
    AC_BRAND_HAIER = 0,  // 海尔空调 (Haier YRW02 / 05)
    AC_BRAND_GREE  = 1,  // 格力空调 (Gree)
    AC_BRAND_MIDEA = 2,  // 美的空调 (Midea)
    AC_BRAND_AUX   = 3,  // 奥克斯空调 (AUX)
} ac_brand_t;

// 运行模式枚举
typedef enum {
    AC_MODE_AUTO = 0,
    AC_MODE_COOL = 1,
    AC_MODE_DRY  = 2,
    AC_MODE_FAN  = 3,
    AC_MODE_HEAT = 4,
} ac_mode_t;

// 风速枚举
typedef enum {
    AC_FAN_AUTO = 0,
    AC_FAN_LOW  = 1,
    AC_FAN_MED  = 2,
    AC_FAN_HIGH = 3,
} ac_fan_t;

// 通用空调控制指令结构体
typedef struct {
    ac_brand_t brand;
    bool power;
    ac_mode_t mode;
    uint8_t temp;       // 16 ~ 30 ℃
    ac_fan_t fan;
    bool swing;
    bool light;
} ac_remote_cmd_t;

// 脉冲生成公共工具函数
size_t ir_protocol_append_byte(rmt_symbol_word_t *symbols, size_t index, uint8_t data, 
                                uint32_t mark_us, uint32_t space0_us, uint32_t space1_us);

// 16 进制 Hex 原始帧格式化打印工具
void ir_protocol_print_hex(const char *brand_name, const uint8_t *bytes, size_t len);

#ifdef __cplusplus
}
#endif

#endif // IR_PROTOCOL_H

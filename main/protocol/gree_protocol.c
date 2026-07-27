/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gree_protocol.h"
#include <stdio.h>

#define GREE_HEADER_MARK_US   9000
#define GREE_HEADER_SPACE_US  4500
#define GREE_BIT_MARK_US      560
#define GREE_BIT_0_SPACE_US   560
#define GREE_BIT_1_SPACE_US   1690

esp_err_t gree_protocol_encode(const ac_remote_cmd_t *cmd, 
                               rmt_symbol_word_t *symbols, 
                               size_t symbol_max, 
                               size_t *out_count)
{
    if (!cmd || !symbols || !out_count || symbol_max < 45) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[2] = {0x00, 0x20};
    if (cmd->mode == AC_MODE_COOL) bytes[0] |= 0x01;
    else if (cmd->mode == AC_MODE_HEAT) bytes[0] |= 0x04;
    if (cmd->power) bytes[0] |= 0x08;

    uint8_t temp_val = (cmd->temp >= 16 && cmd->temp <= 30) ? (cmd->temp - 16) : 9;
    bytes[0] |= (temp_val << 4);

    // 格式化打印格力 16 进制原始数据帧
    ir_protocol_print_hex("格力空调", bytes, 2);

    size_t idx = 0;
    symbols[idx].duration0 = GREE_HEADER_MARK_US;  symbols[idx].level0 = 1;
    symbols[idx].duration1 = GREE_HEADER_SPACE_US; symbols[idx].level1 = 0;
    idx++;

    idx = ir_protocol_append_byte(symbols, idx, bytes[0], GREE_BIT_MARK_US, GREE_BIT_0_SPACE_US, GREE_BIT_1_SPACE_US);
    idx = ir_protocol_append_byte(symbols, idx, bytes[1], GREE_BIT_MARK_US, GREE_BIT_0_SPACE_US, GREE_BIT_1_SPACE_US);

    symbols[idx].duration0 = GREE_BIT_MARK_US; symbols[idx].level0 = 1;
    symbols[idx].duration1 = GREE_BIT_MARK_US; symbols[idx].level1 = 0;
    idx++;

    *out_count = idx;
    return ESP_OK;
}

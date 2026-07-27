/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_protocol.h"
#include <stdio.h>

size_t ir_protocol_append_byte(rmt_symbol_word_t *symbols, size_t index, uint8_t data, 
                                uint32_t mark_us, uint32_t space0_us, uint32_t space1_us)
{
    for (int i = 0; i < 8; i++) {
        bool bit = (data >> (7 - i)) & 0x01; // MSB First
        symbols[index].duration0 = mark_us;
        symbols[index].level0 = 1;
        symbols[index].duration1 = bit ? space1_us : space0_us;
        symbols[index].level1 = 0;
        index++;
    }
    return index;
}

void ir_protocol_print_hex(const char *brand_name, const uint8_t *bytes, size_t len)
{
    if (!bytes || len == 0) return;

    printf("\n========================================================================\n");
    printf("📦 [%s纯 C 协议组帧] 16 进制原始 Hex 数据帧 (%zu 字节):\n   ", brand_name ? brand_name : "空调", len);
    for (size_t i = 0; i < len; i++) {
        printf("0x%02X ", bytes[i]);
    }
    printf("\n========================================================================\n\n");
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_protocol.h"
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "proto";

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

    char hex[3 * 64 + 1] = {0};
    size_t pos = 0;
    size_t max_len = (len < 64) ? len : 64;
    for (size_t i = 0; i < max_len && pos < sizeof(hex); i++) {
        int written = snprintf(hex + pos, sizeof(hex) - pos, "%02X ", bytes[i]);
        if (written < 0 || (size_t)written >= sizeof(hex) - pos) {
            break;
        }
        pos += (size_t)written;
    }
    ESP_LOGI(TAG, "%s frame len=%zu data=%s%s",
             brand_name ? brand_name : "ac", len, hex, (len > max_len) ? "..." : "");
}

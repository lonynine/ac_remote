/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gree_protocol.h"
#include "ir_protocol.h"

#define GREE_HEADER_MARK_US 9000
#define GREE_HEADER_SPACE_US 4500
#define GREE_BIT_MARK_US 560
#define GREE_BIT_0_SPACE_US 560
#define GREE_BIT_1_SPACE_US 1690

esp_err_t gree_protocol_encode(const ac_request_t *request, ir_frame_t *frame)
{
    if (!request || !frame || request->action != AC_ACTION_SET_STATE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[2] = {0x00, 0x20};
    if (request->mode == AC_MODE_COOL) bytes[0] |= 0x01;
    else if (request->mode == AC_MODE_HEAT) bytes[0] |= 0x04;
    if (request->power) bytes[0] |= 0x08;
    uint8_t temp = (request->temp >= 16 && request->temp <= 30) ?
                   request->temp - 16 : 9;
    bytes[0] |= temp << 4;

    ir_protocol_print_hex("格力", bytes, sizeof(bytes));
    size_t index = 0;
    frame->symbols[index++] = (rmt_symbol_word_t) {
        .duration0 = GREE_HEADER_MARK_US, .level0 = 1,
        .duration1 = GREE_HEADER_SPACE_US, .level1 = 0,
    };
    for (size_t i = 0; i < sizeof(bytes); i++) {
        index = ir_protocol_append_byte(frame->symbols, index, bytes[i],
                                        GREE_BIT_MARK_US, GREE_BIT_0_SPACE_US,
                                        GREE_BIT_1_SPACE_US);
    }
    frame->symbols[index++] = (rmt_symbol_word_t) {
        .duration0 = GREE_BIT_MARK_US, .level0 = 1,
        .duration1 = GREE_BIT_MARK_US, .level1 = 0,
    };
    frame->symbol_count = index;
    frame->carrier_hz = 38000;
    return ESP_OK;
}

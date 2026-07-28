/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "haier_protocol.h"
#include <string.h>
#include "ir_protocol.h"

#define HAIER_HEADER_MARK_US 3080
#define HAIER_HEADER_SPACE1_US 3080
#define HAIER_HEADER_SPACE2_US 4500
#define HAIER_BIT_MARK_US 550
#define HAIER_BIT_0_SPACE_US 530
#define HAIER_BIT_1_SPACE_US 1700
#define HAIER_YRW02_SYMBOL_COUNT (2 + HAIER_YRW02_FRAME_LEN * 8 + 1)
#define HAIER_YRW02_MODEL_A 0xA6
#define HAIER_YRW02_POWER_BIT 0x40
#define HAIER_YRW02_SWING_V_DEFAULT 0x0A
#define HAIER_YRW02_BUTTON_POWER 0x05
#define HAIER_YRW02_BUTTON_TIMER 0x10

static uint8_t haier_yrw02_temp_code(uint8_t temp)
{
    return (temp >= 16 && temp <= 30) ? temp - 16 : 9;
}

static uint8_t haier_yrw02_mode_code(ac_mode_t mode)
{
    switch (mode) {
    case AC_MODE_AUTO: return 0;
    case AC_MODE_COOL: return 1;
    case AC_MODE_DRY: return 2;
    case AC_MODE_HEAT: return 4;
    case AC_MODE_FAN: return 6;
    default: return 1;
    }
}

static uint8_t haier_yrw02_fan_code(ac_fan_t fan)
{
    switch (fan) {
    case AC_FAN_HIGH: return 1;
    case AC_FAN_MED: return 2;
    case AC_FAN_LOW: return 3;
    case AC_FAN_AUTO: return 5;
    default: return 3;
    }
}

uint8_t haier_yrw02_checksum(const uint8_t *bytes, size_t len)
{
    uint16_t sum = 0;
    if (!bytes) return 0;
    for (size_t i = 0; i < len; i++) sum += bytes[i];
    return (uint8_t)sum;
}

esp_err_t haier_yrw02_build_frame(const ac_request_t *request,
                                  uint8_t frame[HAIER_YRW02_FRAME_LEN])
{
    if (!request || !frame || request->timer_minutes > 1439) {
        return ESP_ERR_INVALID_ARG;
    }

    ac_timer_mode_t timer_mode = AC_TIMER_NONE;
    uint16_t on_timer_min = 0;
    uint16_t off_timer_min = 0;
    if (request->action == AC_ACTION_TIMER_ON) {
        timer_mode = AC_TIMER_ON;
        on_timer_min = request->timer_minutes;
    } else if (request->action == AC_ACTION_TIMER_OFF) {
        timer_mode = AC_TIMER_OFF;
        off_timer_min = request->timer_minutes;
    }

    memset(frame, 0, HAIER_YRW02_FRAME_LEN);
    frame[0] = HAIER_YRW02_MODEL_A;
    frame[1] = (haier_yrw02_temp_code(request->temp) << 4) |
               HAIER_YRW02_SWING_V_DEFAULT;
    frame[3] = (uint8_t)timer_mode << 5;
    frame[4] = request->power ? HAIER_YRW02_POWER_BIT : 0x00;
    frame[5] = (haier_yrw02_fan_code(request->fan) << 5) |
               (off_timer_min / 60);
    frame[6] = off_timer_min % 60;
    frame[7] = (haier_yrw02_mode_code(request->mode) << 5) |
               (on_timer_min / 60);
    frame[8] = on_timer_min % 60;
    frame[12] = request->action == AC_ACTION_SET_STATE ?
                HAIER_YRW02_BUTTON_POWER : HAIER_YRW02_BUTTON_TIMER;
    frame[13] = haier_yrw02_checksum(frame, HAIER_YRW02_FRAME_LEN - 1);
    return ESP_OK;
}

esp_err_t haier_protocol_encode(const ac_request_t *request, ir_frame_t *frame)
{
    if (!request || !frame || AC_PROTOCOL_MAX_SYMBOLS < HAIER_YRW02_SYMBOL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[HAIER_YRW02_FRAME_LEN];
    esp_err_t err = haier_yrw02_build_frame(request, bytes);
    if (err != ESP_OK) return err;

    ir_protocol_print_hex("海尔 YRW02", bytes, HAIER_YRW02_FRAME_LEN);
    size_t index = 0;
    frame->symbols[index++] = (rmt_symbol_word_t) {
        .duration0 = HAIER_HEADER_MARK_US, .level0 = 1,
        .duration1 = HAIER_HEADER_SPACE1_US, .level1 = 0,
    };
    frame->symbols[index++] = (rmt_symbol_word_t) {
        .duration0 = HAIER_HEADER_MARK_US, .level0 = 1,
        .duration1 = HAIER_HEADER_SPACE2_US, .level1 = 0,
    };
    for (size_t i = 0; i < HAIER_YRW02_FRAME_LEN; i++) {
        index = ir_protocol_append_byte(frame->symbols, index, bytes[i],
                                        HAIER_BIT_MARK_US, HAIER_BIT_0_SPACE_US,
                                        HAIER_BIT_1_SPACE_US);
    }
    frame->symbols[index++] = (rmt_symbol_word_t) {
        .duration0 = HAIER_BIT_MARK_US, .level0 = 1,
        .duration1 = 0, .level1 = 0,
    };
    frame->symbol_count = index;
    frame->carrier_hz = 38000;
    return ESP_OK;
}

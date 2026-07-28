/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROTOCOL_TYPES_H
#define PROTOCOL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/rmt_types.h"
#include "ac_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_PROTOCOL_MAX_SYMBOLS 256

typedef enum {
    AC_ACTION_SET_STATE = 0,
    AC_ACTION_TIMER_ON,
    AC_ACTION_TIMER_OFF,
    AC_ACTION_TIMER_CANCEL,
} ac_action_t;

typedef struct {
    ac_brand_t brand;
    ac_action_t action;
    bool power;
    ac_mode_t mode;
    uint8_t temp;
    ac_fan_t fan;
    bool swing;
    bool light;
    uint16_t timer_minutes;
} ac_request_t;

typedef struct {
    rmt_symbol_word_t symbols[AC_PROTOCOL_MAX_SYMBOLS];
    size_t symbol_count;
    uint32_t carrier_hz;
} ir_frame_t;

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_TYPES_H

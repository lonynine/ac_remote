/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AC_STATE_H
#define AC_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "ac_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool power;
    ac_mode_t mode;
    uint8_t temp;
    ac_fan_t fan;
    bool swing;
    bool light;
    uint16_t timer_min;
} ac_state_t;

esp_err_t ac_state_init(void);
esp_err_t ac_state_get(ac_state_t *state);
esp_err_t ac_state_set(const ac_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // AC_STATE_H

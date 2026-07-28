/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include "esp_err.h"
#include "protocol_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AC_PROTOCOL_CAP_SET_STATE    = 1U << 0,
    AC_PROTOCOL_CAP_TIMER_ON     = 1U << 1,
    AC_PROTOCOL_CAP_TIMER_OFF    = 1U << 2,
    AC_PROTOCOL_CAP_TIMER_CANCEL = 1U << 3,
} ac_protocol_capability_t;

typedef struct {
    ac_brand_t brand;
    const char *name;
    uint32_t capabilities;
    esp_err_t (*encode)(const ac_request_t *request, ir_frame_t *frame);
} ac_protocol_ops_t;

esp_err_t ac_protocol_encode(const ac_request_t *request, ir_frame_t *frame);
bool ac_protocol_supports(ac_brand_t brand, ac_action_t action);
const char *ac_protocol_brand_name(ac_brand_t brand);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_MANAGER_H

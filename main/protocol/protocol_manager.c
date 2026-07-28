/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protocol_manager.h"
#include <string.h>
#include "haier/haier_protocol.h"
#include "gree/gree_protocol.h"

static const ac_protocol_ops_t s_protocols[] = {
    {
        .brand = AC_BRAND_HAIER,
        .name = "haier",
        .capabilities = AC_PROTOCOL_CAP_SET_STATE |
                        AC_PROTOCOL_CAP_TIMER_ON |
                        AC_PROTOCOL_CAP_TIMER_OFF |
                        AC_PROTOCOL_CAP_TIMER_CANCEL,
        .encode = haier_protocol_encode,
    },
    {
        .brand = AC_BRAND_GREE,
        .name = "gree",
        .capabilities = AC_PROTOCOL_CAP_SET_STATE,
        .encode = gree_protocol_encode,
    },
};

static uint32_t action_capability(ac_action_t action)
{
    switch (action) {
    case AC_ACTION_SET_STATE: return AC_PROTOCOL_CAP_SET_STATE;
    case AC_ACTION_TIMER_ON: return AC_PROTOCOL_CAP_TIMER_ON;
    case AC_ACTION_TIMER_OFF: return AC_PROTOCOL_CAP_TIMER_OFF;
    case AC_ACTION_TIMER_CANCEL: return AC_PROTOCOL_CAP_TIMER_CANCEL;
    default: return 0;
    }
}

static const ac_protocol_ops_t *find_protocol(ac_brand_t brand)
{
    for (size_t i = 0; i < sizeof(s_protocols) / sizeof(s_protocols[0]); i++) {
        if (s_protocols[i].brand == brand) {
            return &s_protocols[i];
        }
    }
    return NULL;
}

esp_err_t ac_protocol_encode(const ac_request_t *request, ir_frame_t *frame)
{
    if (!request || !frame) {
        return ESP_ERR_INVALID_ARG;
    }

    const ac_protocol_ops_t *protocol = find_protocol(request->brand);
    uint32_t capability = action_capability(request->action);
    if (!protocol || capability == 0 || !(protocol->capabilities & capability)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (request->mode > AC_MODE_HEAT || request->fan > AC_FAN_HIGH ||
        request->temp < 16 || request->temp > 30) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((request->action == AC_ACTION_TIMER_ON ||
         request->action == AC_ACTION_TIMER_OFF) &&
        (request->timer_minutes == 0 || request->timer_minutes > 1439)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(frame, 0, sizeof(*frame));
    esp_err_t err = protocol->encode(request, frame);
    if (err != ESP_OK) {
        return err;
    }
    if (frame->symbol_count == 0 ||
        frame->symbol_count > AC_PROTOCOL_MAX_SYMBOLS || frame->carrier_hz == 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

bool ac_protocol_supports(ac_brand_t brand, ac_action_t action)
{
    const ac_protocol_ops_t *protocol = find_protocol(brand);
    uint32_t capability = action_capability(action);
    return protocol && capability != 0 && (protocol->capabilities & capability) != 0;
}

const char *ac_protocol_brand_name(ac_brand_t brand)
{
    const ac_protocol_ops_t *protocol = find_protocol(brand);
    return protocol ? protocol->name : "unknown";
}

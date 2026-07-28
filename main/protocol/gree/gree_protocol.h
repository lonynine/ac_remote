/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GREE_PROTOCOL_H
#define GREE_PROTOCOL_H

#include "esp_err.h"
#include "protocol_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gree_protocol_encode(const ac_request_t *request, ir_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif // GREE_PROTOCOL_H

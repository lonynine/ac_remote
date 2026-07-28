/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAIER_PROTOCOL_H
#define HAIER_PROTOCOL_H

#include "esp_err.h"
#include "protocol_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAIER_YRW02_FRAME_LEN 14

esp_err_t haier_yrw02_build_frame(const ac_request_t *request,
                                  uint8_t frame[HAIER_YRW02_FRAME_LEN]);
uint8_t haier_yrw02_checksum(const uint8_t *bytes, size_t len);
esp_err_t haier_protocol_encode(const ac_request_t *request, ir_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif // HAIER_PROTOCOL_H

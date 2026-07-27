/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GREE_PROTOCOL_H
#define GREE_PROTOCOL_H

#include "ir_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gree_protocol_encode(const ac_remote_cmd_t *cmd, 
                               rmt_symbol_word_t *symbols, 
                               size_t symbol_max, 
                               size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif // GREE_PROTOCOL_H

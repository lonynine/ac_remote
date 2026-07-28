/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IR_LEARN_H
#define IR_LEARN_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ir_learn_init(void);
esp_err_t ir_learn_start(uint32_t timeout_sec);
esp_err_t ir_learn_emit(void);

#ifdef __cplusplus
}
#endif

#endif // IR_LEARN_H

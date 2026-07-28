/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IR_DRIVER_H
#define IR_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/rmt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*ir_driver_rx_done_cb_t)(const rmt_symbol_word_t *symbols,
                                       size_t num_symbols,
                                       bool is_last,
                                       void *user_data);

esp_err_t ir_driver_tx_init(gpio_num_t gpio_num, uint32_t carrier_freq_hz);
esp_err_t ir_driver_tx_symbols(const rmt_symbol_word_t *symbols, size_t count);

esp_err_t ir_driver_rx_init(gpio_num_t gpio_num);
esp_err_t ir_driver_rx_register_done_callback(ir_driver_rx_done_cb_t cb, void *user_data);
esp_err_t ir_driver_rx_start(rmt_symbol_word_t *symbols_buf, size_t buf_capacity);
esp_err_t ir_driver_rx_stop(void);

/* Compatibility wrapper: starts one async receive job; RX data is reported by callback users. */
esp_err_t ir_driver_rx_receive(rmt_symbol_word_t *symbols_buf,
                               size_t buf_capacity,
                               size_t *out_count,
                               uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // IR_DRIVER_H

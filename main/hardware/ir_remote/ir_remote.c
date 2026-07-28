/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_remote.h"
#include "ir_driver.h"
#include "ir_learn.h"
#include "esp_log.h"

static const char *TAG = "ir";

static void log_sent_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    ESP_LOGI(TAG, "tx waveform symbols=%zu", count);
    for (size_t i = 0; i < count; i++) {
        ESP_LOGI(TAG, "tx[%02zu] mark=%uus space=%uus",
                 i, symbols[i].duration0, symbols[i].duration1);
    }
}

esp_err_t ir_remote_init(void)
{
    esp_err_t err = ir_driver_tx_init(IR_TX_GPIO_NUM, 38000);
    if (err != ESP_OK) {
        return err;
    }

    err = ir_driver_rx_init(IR_RX_GPIO_NUM);
    if (err != ESP_OK) {
        return err;
    }

    err = ir_learn_init();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "init tx_gpio=%d rx_gpio=%d", IR_TX_GPIO_NUM, IR_RX_GPIO_NUM);
    return ESP_OK;
}

esp_err_t ir_remote_send_frame(const ir_frame_t *frame)
{
    if (!frame || frame->symbol_count == 0 ||
        frame->symbol_count > AC_PROTOCOL_MAX_SYMBOLS || frame->carrier_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ir_driver_tx_set_carrier(frame->carrier_hz);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set carrier failed frequency=%luHz err=%s",
                 (unsigned long)frame->carrier_hz, esp_err_to_name(err));
        return err;
    }

    err = ir_driver_tx_symbols(frame->symbols, frame->symbol_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "send frame failed symbols=%zu err=%s",
                 frame->symbol_count, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "send frame carrier=%luHz symbols=%zu",
             (unsigned long)frame->carrier_hz, frame->symbol_count);
    log_sent_symbols(frame->symbols, frame->symbol_count);
    return ESP_OK;
}

esp_err_t ir_remote_learn_start(uint32_t timeout_sec)
{
    return ir_learn_start(timeout_sec);
}

esp_err_t ir_remote_learn_emit(void)
{
    return ir_learn_emit();
}

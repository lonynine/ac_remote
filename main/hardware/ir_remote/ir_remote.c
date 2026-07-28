/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_remote.h"
#include "ir_driver.h"
#include "ir_learn.h"
#include "haier_protocol.h"
#include "gree_protocol.h"
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

esp_err_t ir_remote_send_cmd(const ac_remote_cmd_t *cmd)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    rmt_symbol_word_t symbols[140];
    size_t symbol_count = 0;
    esp_err_t err = ESP_OK;

    switch (cmd->brand) {
    case AC_BRAND_HAIER:
        err = haier_protocol_encode(cmd, symbols, 140, &symbol_count);
        break;
    case AC_BRAND_GREE:
        err = gree_protocol_encode(cmd, symbols, 140, &symbol_count);
        break;
    default:
        err = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "encode failed brand=%d err=%s", cmd->brand, esp_err_to_name(err));
        return err;
    }

    err = ir_driver_tx_symbols(symbols, symbol_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "send failed brand=%d symbols=%zu err=%s",
                 cmd->brand, symbol_count, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "send brand=%d power=%d mode=%d temp=%u fan=%d symbols=%zu",
             cmd->brand, cmd->power, cmd->mode, cmd->temp, cmd->fan, symbol_count);
    log_sent_symbols(symbols, symbol_count);
    return ESP_OK;
}

esp_err_t ir_remote_send_haier(const haier_ac_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    ac_remote_cmd_t cmd = *status;
    cmd.brand = AC_BRAND_HAIER;
    return ir_remote_send_cmd(&cmd);
}

esp_err_t ir_remote_send_gree(const gree_ac_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    ac_remote_cmd_t cmd = *status;
    cmd.brand = AC_BRAND_GREE;
    return ir_remote_send_cmd(&cmd);
}

esp_err_t ir_remote_learn_start(uint32_t timeout_sec)
{
    return ir_learn_start(timeout_sec);
}

esp_err_t ir_remote_learn_emit(void)
{
    return ir_learn_emit();
}

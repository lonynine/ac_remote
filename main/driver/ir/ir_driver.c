/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_driver.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"

static const char *TAG = "rmt";

#define IR_TX_DONE_TIMEOUT_MS 2000
#define IR_TX_CARRIER_DUTY 0.33f
#define IR_RX_MIN_NS 1250
#define IR_RX_MAX_NS 20000000

static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static rmt_channel_handle_t s_rx_channel = NULL;
static uint32_t s_tx_carrier_freq_hz = 0;

static ir_driver_rx_done_cb_t s_rx_done_cb = NULL;
static void *s_rx_done_user_data = NULL;

static bool IRAM_ATTR ir_driver_rmt_rx_done_cb(rmt_channel_handle_t channel,
                                               const rmt_rx_done_event_data_t *edata,
                                               void *user_data)
{
    (void)channel;
    (void)user_data;

    if (!s_rx_done_cb || !edata) {
        return false;
    }

    return s_rx_done_cb(edata->received_symbols,
                        edata->num_symbols,
                        edata->flags.is_last,
                        s_rx_done_user_data);
}

static esp_err_t ir_driver_rx_enable(void)
{
    if (!s_rx_channel) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = rmt_enable(s_rx_channel);
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable rx failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ir_driver_tx_init(gpio_num_t gpio_num, uint32_t carrier_freq_hz)
{
    if (s_tx_channel != NULL) {
        return ESP_OK;
    }

    rmt_tx_channel_config_t tx_alloc_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&tx_alloc_config, &s_tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new tx channel failed: %s", esp_err_to_name(err));
        return err;
    }

    rmt_carrier_config_t carrier_config = {
        .duty_cycle = IR_TX_CARRIER_DUTY,
        .frequency_hz = carrier_freq_hz,
        .flags.polarity_active_low = false,
    };
    err = rmt_apply_carrier(s_tx_channel, &carrier_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "apply carrier failed: %s", esp_err_to_name(err));
        return err;
    }
    s_tx_carrier_freq_hz = carrier_freq_hz;

    rmt_copy_encoder_config_t copy_encoder_config = {};
    err = rmt_new_copy_encoder(&copy_encoder_config, &s_copy_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new copy encoder failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_enable(s_tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable tx failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "tx init gpio=%d carrier=%luHz duty=33%%",
             gpio_num, (unsigned long)carrier_freq_hz);
    return ESP_OK;
}

esp_err_t ir_driver_tx_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    if (!s_tx_channel || !s_copy_encoder) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!symbols || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(s_tx_channel, s_copy_encoder, symbols,
                                 sizeof(rmt_symbol_word_t) * count, &transmit_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tx start failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_tx_wait_all_done(s_tx_channel, IR_TX_DONE_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tx wait failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "tx done symbols=%zu carrier=%luHz duty=33%%",
             count, (unsigned long)s_tx_carrier_freq_hz);
    return ESP_OK;
}

esp_err_t ir_driver_tx_set_carrier(uint32_t carrier_freq_hz)
{
    if (!s_tx_channel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (carrier_freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (carrier_freq_hz == s_tx_carrier_freq_hz) {
        return ESP_OK;
    }

    const rmt_carrier_config_t carrier_config = {
        .duty_cycle = IR_TX_CARRIER_DUTY,
        .frequency_hz = carrier_freq_hz,
        .flags.polarity_active_low = false,
    };
    esp_err_t err = rmt_apply_carrier(s_tx_channel, &carrier_config);
    if (err == ESP_OK) {
        s_tx_carrier_freq_hz = carrier_freq_hz;
    }
    return err;
}

esp_err_t ir_driver_rx_init(gpio_num_t gpio_num)
{
    if (s_rx_channel != NULL) {
        return ESP_OK;
    }

    rmt_rx_channel_config_t rx_alloc_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 128,
        .resolution_hz = 1000000,
    };
    esp_err_t err = rmt_new_rx_channel(&rx_alloc_config, &s_rx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new rx channel failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "rx init gpio=%d", gpio_num);
    return ESP_OK;
}

esp_err_t ir_driver_rx_register_done_callback(ir_driver_rx_done_cb_t cb, void *user_data)
{
    if (!s_rx_channel) {
        return ESP_ERR_INVALID_STATE;
    }

    s_rx_done_cb = cb;
    s_rx_done_user_data = user_data;

    rmt_rx_event_callbacks_t callbacks = {
        .on_recv_done = cb ? ir_driver_rmt_rx_done_cb : NULL,
    };
    esp_err_t err = rmt_rx_register_event_callbacks(s_rx_channel, &callbacks, NULL);
    if (err != ESP_OK) {
        s_rx_done_cb = NULL;
        s_rx_done_user_data = NULL;
        ESP_LOGE(TAG, "register rx callback failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ir_driver_rx_start(rmt_symbol_word_t *symbols_buf, size_t buf_capacity)
{
    if (!s_rx_channel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!symbols_buf || buf_capacity == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ir_driver_rx_enable();
    if (err != ESP_OK) {
        return err;
    }

    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = IR_RX_MIN_NS,
        .signal_range_max_ns = IR_RX_MAX_NS,
    };

    err = rmt_receive(s_rx_channel,
                      symbols_buf,
                      sizeof(rmt_symbol_word_t) * buf_capacity,
                      &receive_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rx start failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ir_driver_rx_stop(void)
{
    if (!s_rx_channel) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = rmt_disable(s_rx_channel);
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "disable rx failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ir_driver_rx_receive(rmt_symbol_word_t *symbols_buf,
                               size_t buf_capacity,
                               size_t *out_count,
                               uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (!out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0;

    return ir_driver_rx_start(symbols_buf, buf_capacity);
}

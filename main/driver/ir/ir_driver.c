/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_driver.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

static const char *TAG = "ir_driver";

static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static rmt_channel_handle_t s_rx_channel = NULL;

esp_err_t ir_driver_tx_init(gpio_num_t gpio_num, uint32_t carrier_freq_hz)
{
    if (s_tx_channel != NULL) {
        return ESP_OK; // 已初始化
    }

    // 1. 创建 RMT TX 发送通道
    rmt_tx_channel_config_t tx_alloc_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000, // 1MHz 分辨率 (1us 精度)
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_alloc_config, &s_tx_channel));

    // 2. 应用 38kHz 载波调制 (工业标准占空比 33%)
    rmt_carrier_config_t carrier_config = {
        .duty_cycle = 0.33,
        .frequency_hz = carrier_freq_hz,
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(s_tx_channel, &carrier_config));

    // 3. 创建 RMT Copy Encoder
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &s_copy_encoder));

    // 4. 使能 TX 通道
    ESP_ERROR_CHECK(rmt_enable(s_tx_channel));

    ESP_LOGI(TAG, "RMT 纯发送总线驱动初始化成功 (GPIO: %d, 载波: %lu Hz)", gpio_num, (unsigned long)carrier_freq_hz);
    return ESP_OK;
}

esp_err_t ir_driver_tx_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    if (!s_tx_channel || !s_copy_encoder || !symbols || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    return rmt_transmit(s_tx_channel, s_copy_encoder, symbols, sizeof(rmt_symbol_word_t) * count, &transmit_config);
}

esp_err_t ir_driver_rx_init(gpio_num_t gpio_num)
{
    if (s_rx_channel != NULL) {
        return ESP_OK;
    }

    rmt_rx_channel_config_t rx_alloc_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 128, // 扩展 RMT 内存块容纳长帧波形
        .resolution_hz = 1000000, // 1us 精度
    };
    esp_err_t err = rmt_new_rx_channel(&rx_alloc_config, &s_rx_channel);
    if (err == ESP_OK) {
        rmt_enable(s_rx_channel);
        ESP_LOGI(TAG, "RMT 纯接收总线驱动初始化成功 (GPIO: %d)", gpio_num);
    }
    return err;
}

esp_err_t ir_driver_rx_receive(rmt_symbol_word_t *symbols_buf, size_t buf_capacity, size_t *out_count, uint32_t timeout_ms)
{
    if (!s_rx_channel || !symbols_buf || buf_capacity == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 重新开启使能 (防止上一轮接收结束后被 auto disable)
    rmt_enable(s_rx_channel);

    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 1250,     // 过滤 <1.25us 噪点
        .signal_range_max_ns = 20000000, // 放宽至 20ms 空闲判断
    };

    return rmt_receive(s_rx_channel, symbols_buf, sizeof(rmt_symbol_word_t) * buf_capacity, &receive_config);
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_learn.h"
#include "ir_driver.h"
#include <stdio.h>
#include <string.h>
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "ir";

#define LEARN_BUF_MAX 256
#define LEARN_QUEUE_LEN 4
#define LEARN_MIN_SYMBOLS 10
#define LEARN_DEFAULT_TIMEOUT_SEC 10

typedef struct {
    size_t count;
    bool is_last;
    bool truncated;
} ir_learn_rx_event_t;

static QueueHandle_t s_learn_queue = NULL;
static volatile bool s_learning_active = false;
static bool s_learn_initialized = false;

static rmt_symbol_word_t s_rx_symbols[LEARN_BUF_MAX];
static rmt_symbol_word_t s_learned_symbols[LEARN_BUF_MAX];
static size_t s_learned_count = 0;

static TickType_t learn_timeout_ticks(uint32_t timeout_sec)
{
    if (timeout_sec == 0) {
        timeout_sec = LEARN_DEFAULT_TIMEOUT_SEC;
    }

    uint64_t timeout_ms = (uint64_t)timeout_sec * 1000ULL;
    if (timeout_ms > UINT32_MAX) {
        timeout_ms = UINT32_MAX;
    }

    TickType_t ticks = pdMS_TO_TICKS((uint32_t)timeout_ms);
    return ticks == 0 ? 1 : ticks;
}

static void decode_and_log_hex(const rmt_symbol_word_t *symbols, size_t count)
{
    if (count < LEARN_MIN_SYMBOLS) {
        return;
    }

    size_t start_idx = 0;
    while (start_idx < count &&
           (symbols[start_idx].duration0 > 2000 || symbols[start_idx].duration1 > 2000)) {
        start_idx++;
    }
    if (start_idx >= count) {
        return;
    }

    uint8_t current_byte = 0;
    int bit_count = 0;
    uint8_t hex_bytes[64] = {0};
    size_t byte_count = 0;

    for (size_t i = start_idx; i < count; i++) {
        if (symbols[i].duration0 == 0) {
            break;
        }

        bool bit = symbols[i].duration1 > 1000;
        current_byte = (current_byte << 1) | (bit ? 1 : 0);
        bit_count++;

        if (bit_count == 8) {
            if (byte_count < sizeof(hex_bytes)) {
                hex_bytes[byte_count++] = current_byte;
            }
            current_byte = 0;
            bit_count = 0;
        }
    }

    if (byte_count == 0) {
        return;
    }

    char hex[3 * 64 + 1] = {0};
    size_t pos = 0;
    for (size_t i = 0; i < byte_count && pos < sizeof(hex); i++) {
        int written = snprintf(hex + pos, sizeof(hex) - pos, "%02X ", hex_bytes[i]);
        if (written < 0 || (size_t)written >= sizeof(hex) - pos) {
            break;
        }
        pos += (size_t)written;
    }

    ESP_LOGI(TAG, "learn hex guess len=%zu data=%s", byte_count, hex);
}

static void log_raw_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    decode_and_log_hex(symbols, count);

    ESP_LOGI(TAG, "learn rx waveform symbols=%zu", count);
    for (size_t i = 0; i < count; i++) {
        ESP_LOGI(TAG, "learn rx[%02zu] mark=%uus space=%uus",
                 i, symbols[i].duration0, symbols[i].duration1);
    }
}

static esp_err_t arm_next_receive(void)
{
    memset(s_rx_symbols, 0, sizeof(s_rx_symbols));
    return ir_driver_rx_start(s_rx_symbols, LEARN_BUF_MAX);
}

static bool IRAM_ATTR ir_learn_rx_done_cb(const rmt_symbol_word_t *symbols,
                                          size_t num_symbols,
                                          bool is_last,
                                          void *user_data)
{
    (void)symbols;
    (void)user_data;

    if (!s_learning_active || !s_learn_queue) {
        return false;
    }

    ir_learn_rx_event_t event = {
        .count = num_symbols,
        .is_last = is_last,
        .truncated = num_symbols >= LEARN_BUF_MAX,
    };

    BaseType_t high_task_wakeup = pdFALSE;
    if (xQueueSendFromISR(s_learn_queue, &event, &high_task_wakeup) != pdTRUE) {
        return false;
    }
    return high_task_wakeup == pdTRUE;
}

esp_err_t ir_learn_init(void)
{
    if (s_learn_initialized) {
        return ESP_OK;
    }

    if (s_learn_queue == NULL) {
        s_learn_queue = xQueueCreate(LEARN_QUEUE_LEN, sizeof(ir_learn_rx_event_t));
        if (s_learn_queue == NULL) {
            ESP_LOGE(TAG, "learn queue create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = ir_driver_rx_register_done_callback(ir_learn_rx_done_cb, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "learn callback register failed: %s", esp_err_to_name(err));
        return err;
    }

    s_learn_initialized = true;
    return ESP_OK;
}

esp_err_t ir_learn_start(uint32_t timeout_sec)
{
    if (s_learning_active) {
        ESP_LOGW(TAG, "learn already active");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ir_learn_init();
    if (err != ESP_OK) {
        return err;
    }

    if (timeout_sec == 0) {
        timeout_sec = LEARN_DEFAULT_TIMEOUT_SEC;
    }

    xQueueReset(s_learn_queue);
    s_learning_active = true;

    err = arm_next_receive();
    if (err != ESP_OK) {
        s_learning_active = false;
        ir_driver_rx_stop();
        return err;
    }

    ESP_LOGI(TAG, "learn start timeout=%lus", (unsigned long)timeout_sec);

    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = learn_timeout_ticks(timeout_sec);
    int capture_index = 1;

    while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
        TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
        TickType_t remaining_ticks = timeout_ticks - elapsed_ticks;
        ir_learn_rx_event_t event = {0};

        if (xQueueReceive(s_learn_queue, &event, remaining_ticks) != pdTRUE) {
            break;
        }

        if (!event.is_last) {
            ESP_LOGW(TAG, "learn ignored partial symbols=%zu", event.count);
            continue;
        }

        bool captured = false;
        size_t count = event.count > LEARN_BUF_MAX ? LEARN_BUF_MAX : event.count;
        if (count < LEARN_MIN_SYMBOLS) {
            ESP_LOGW(TAG, "learn ignored noise symbols=%zu", count);
        } else {
            memcpy(s_learned_symbols, s_rx_symbols, sizeof(rmt_symbol_word_t) * count);
            s_learned_count = count;
            captured = true;
            ESP_LOGI(TAG, "learn capture index=%d symbols=%zu", capture_index++, count);
            if (event.truncated) {
                ESP_LOGW(TAG, "learn capture may be truncated buffer=%d", LEARN_BUF_MAX);
            }
        }

        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            if (captured) {
                log_raw_symbols(s_learned_symbols, s_learned_count);
            }
            break;
        }

        err = arm_next_receive();
        if (err != ESP_OK) {
            s_learning_active = false;
            ir_driver_rx_stop();
            return err;
        }

        if (captured) {
            log_raw_symbols(s_learned_symbols, s_learned_count);
        }
    }

    s_learning_active = false;
    err = ir_driver_rx_stop();
    if (err != ESP_OK) {
        return err;
    }

    if (s_learned_count == 0) {
        ESP_LOGW(TAG, "learn stop without capture");
    } else {
        ESP_LOGI(TAG, "learn stop last_symbols=%zu", s_learned_count);
    }
    return ESP_OK;
}

esp_err_t ir_learn_emit(void)
{
    if (s_learned_count == 0 || s_learned_symbols[0].duration0 == 0) {
        ESP_LOGE(TAG, "learn emit failed: no cached waveform");
        return ESP_ERR_INVALID_STATE;
    }

    rmt_symbol_word_t tx_symbols[LEARN_BUF_MAX];
    for (size_t i = 0; i < s_learned_count; i++) {
        tx_symbols[i].duration0 = s_learned_symbols[i].duration0;
        tx_symbols[i].level0 = 1;
        tx_symbols[i].duration1 = s_learned_symbols[i].duration1;
        tx_symbols[i].level1 = 0;
    }

    ESP_LOGI(TAG, "learn emit symbols=%zu", s_learned_count);
    esp_err_t err = ir_driver_tx_symbols(tx_symbols, s_learned_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "learn emit failed: %s", esp_err_to_name(err));
    }
    return err;
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ac_state.h"
#include "nvs_driver.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ac";

#define NVS_KEY_AC_POWER  "ac_power"
#define NVS_KEY_AC_BRAND  "ac_brand"
#define NVS_KEY_AC_MODE   "ac_mode"
#define NVS_KEY_AC_TEMP   "ac_temp"
#define NVS_KEY_AC_FAN    "ac_fan"
#define NVS_KEY_AC_SWING  "ac_swing"
#define NVS_KEY_AC_LIGHT  "ac_light"
#define NVS_KEY_TIMER_MODE "ac_timer_mode"
#define NVS_KEY_ON_TIMER   "ac_on_timer"
#define NVS_KEY_OFF_TIMER  "ac_off_timer"

static ac_state_t s_ac_state;
static bool s_is_initialized = false;

static const ac_state_t s_default_ac_state = {
    .brand = AC_BRAND_HAIER,
    .power = false,
    .mode  = AC_MODE_COOL,
    .temp  = 26,
    .fan   = AC_FAN_AUTO,
    .swing = true,
    .light = true,
    .timer_mode = AC_TIMER_NONE,
    .on_timer_min = 0,
    .off_timer_min = 0
};

static void log_default_write_result(const char *key, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write default %s failed: %s", key, esp_err_to_name(err));
    }
}

static esp_err_t keep_first_error(esp_err_t current, esp_err_t next)
{
    return (current == ESP_OK) ? next : current;
}

esp_err_t ac_state_init(void)
{
    if (s_is_initialized) {
        return ESP_OK;
    }

    uint8_t u8_val = 0;
    esp_err_t err;

    err = nvs_driver_read_u8(NVS_KEY_AC_BRAND, &u8_val);
    s_ac_state.brand = (err == ESP_OK && u8_val <= AC_BRAND_AUX) ?
                       (ac_brand_t)u8_val : s_default_ac_state.brand;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_BRAND, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_BRAND,
                                 nvs_driver_write_u8(NVS_KEY_AC_BRAND, (uint8_t)s_ac_state.brand));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_POWER, &u8_val);
    s_ac_state.power = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.power;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_POWER, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_POWER,
                                 nvs_driver_write_u8(NVS_KEY_AC_POWER, s_ac_state.power ? 1 : 0));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_MODE, &u8_val);
    s_ac_state.mode = (err == ESP_OK) ? (ac_mode_t)u8_val : s_default_ac_state.mode;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_MODE, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_MODE,
                                 nvs_driver_write_u8(NVS_KEY_AC_MODE, (uint8_t)s_ac_state.mode));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_TEMP, &u8_val);
    s_ac_state.temp = (err == ESP_OK) ? u8_val : s_default_ac_state.temp;
    if (s_ac_state.temp < 16 || s_ac_state.temp > 30) {
        s_ac_state.temp = s_default_ac_state.temp;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_TEMP, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_TEMP,
                                 nvs_driver_write_u8(NVS_KEY_AC_TEMP, s_ac_state.temp));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_FAN, &u8_val);
    s_ac_state.fan = (err == ESP_OK) ? (ac_fan_t)u8_val : s_default_ac_state.fan;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_FAN, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_FAN,
                                 nvs_driver_write_u8(NVS_KEY_AC_FAN, (uint8_t)s_ac_state.fan));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_SWING, &u8_val);
    s_ac_state.swing = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.swing;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_SWING, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_SWING,
                                 nvs_driver_write_u8(NVS_KEY_AC_SWING, s_ac_state.swing ? 1 : 0));
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_LIGHT, &u8_val);
    s_ac_state.light = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.light;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_AC_LIGHT, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_AC_LIGHT,
                                 nvs_driver_write_u8(NVS_KEY_AC_LIGHT, s_ac_state.light ? 1 : 0));
    }

    err = nvs_driver_read_u8(NVS_KEY_TIMER_MODE, &u8_val);
    s_ac_state.timer_mode = (err == ESP_OK) ? (ac_timer_mode_t)u8_val : s_default_ac_state.timer_mode;
    if (s_ac_state.timer_mode != AC_TIMER_NONE &&
        s_ac_state.timer_mode != AC_TIMER_OFF &&
        s_ac_state.timer_mode != AC_TIMER_ON &&
        s_ac_state.timer_mode != AC_TIMER_ON_THEN_OFF &&
        s_ac_state.timer_mode != AC_TIMER_OFF_THEN_ON) {
        s_ac_state.timer_mode = AC_TIMER_NONE;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_TIMER_MODE, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_TIMER_MODE,
                                 nvs_driver_write_u8(NVS_KEY_TIMER_MODE, (uint8_t)s_ac_state.timer_mode));
    }

    uint16_t u16_val = 0;
    err = nvs_driver_read_u16(NVS_KEY_ON_TIMER, &u16_val);
    s_ac_state.on_timer_min = (err == ESP_OK && u16_val <= 1439) ? u16_val : 0;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_ON_TIMER, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_ON_TIMER,
                                 nvs_driver_write_u16(NVS_KEY_ON_TIMER, s_ac_state.on_timer_min));
    }

    err = nvs_driver_read_u16(NVS_KEY_OFF_TIMER, &u16_val);
    s_ac_state.off_timer_min = (err == ESP_OK && u16_val <= 1439) ? u16_val : 0;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read %s failed: %s, use default", NVS_KEY_OFF_TIMER, esp_err_to_name(err));
        log_default_write_result(NVS_KEY_OFF_TIMER,
                                 nvs_driver_write_u16(NVS_KEY_OFF_TIMER, s_ac_state.off_timer_min));
    }

    s_is_initialized = true;
    ESP_LOGI(TAG, "AC state storage initialized");
    return ESP_OK;
}

esp_err_t ac_state_get(ac_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_is_initialized) {
        ac_state_init();
    }
    memcpy(state, &s_ac_state, sizeof(ac_state_t));
    return ESP_OK;
}

esp_err_t ac_state_set(const ac_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_BRAND, (uint8_t)state->brand));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_POWER, state->power ? 1 : 0));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_MODE, (uint8_t)state->mode));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_TEMP, state->temp));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_FAN, (uint8_t)state->fan));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_SWING, state->swing ? 1 : 0));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_AC_LIGHT, state->light ? 1 : 0));
    err = keep_first_error(err, nvs_driver_write_u8(NVS_KEY_TIMER_MODE, (uint8_t)state->timer_mode));
    err = keep_first_error(err, nvs_driver_write_u16(NVS_KEY_ON_TIMER, state->on_timer_min));
    err = keep_first_error(err, nvs_driver_write_u16(NVS_KEY_OFF_TIMER, state->off_timer_min));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save AC state failed: %s", esp_err_to_name(err));
        return err;
    }

    memcpy(&s_ac_state, state, sizeof(ac_state_t));
    return ESP_OK;
}

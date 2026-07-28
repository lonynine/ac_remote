/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ac_state.h"
#include "nvs_driver.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ac_state";

#define NVS_KEY_AC_POWER  "ac_power"
#define NVS_KEY_AC_MODE   "ac_mode"
#define NVS_KEY_AC_TEMP   "ac_temp"
#define NVS_KEY_AC_FAN    "ac_fan"
#define NVS_KEY_AC_SWING  "ac_swing"
#define NVS_KEY_AC_LIGHT  "ac_light"
#define NVS_KEY_AC_TIMER  "ac_timer"

static ac_state_t s_ac_state;
static bool s_is_initialized = false;

static const ac_state_t s_default_ac_state = {
    .power = false,
    .mode  = AC_MODE_COOL,
    .temp  = 26,
    .fan   = AC_FAN_AUTO,
    .swing = true,
    .light = true,
    .timer_min = 0
};

esp_err_t ac_state_init(void)
{
    if (s_is_initialized) {
        return ESP_OK;
    }

    uint8_t u8_val = 0;
    esp_err_t err;

    err = nvs_driver_read_u8(NVS_KEY_AC_POWER, &u8_val);
    s_ac_state.power = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.power;

    err = nvs_driver_read_u8(NVS_KEY_AC_MODE, &u8_val);
    s_ac_state.mode = (err == ESP_OK) ? (ac_mode_t)u8_val : s_default_ac_state.mode;

    err = nvs_driver_read_u8(NVS_KEY_AC_TEMP, &u8_val);
    s_ac_state.temp = (err == ESP_OK) ? u8_val : s_default_ac_state.temp;
    if (s_ac_state.temp < 16 || s_ac_state.temp > 30) {
        s_ac_state.temp = s_default_ac_state.temp;
    }

    err = nvs_driver_read_u8(NVS_KEY_AC_FAN, &u8_val);
    s_ac_state.fan = (err == ESP_OK) ? (ac_fan_t)u8_val : s_default_ac_state.fan;

    err = nvs_driver_read_u8(NVS_KEY_AC_SWING, &u8_val);
    s_ac_state.swing = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.swing;

    err = nvs_driver_read_u8(NVS_KEY_AC_LIGHT, &u8_val);
    s_ac_state.light = (err == ESP_OK) ? (u8_val != 0) : s_default_ac_state.light;

    uint16_t u16_val = 0;
    err = nvs_driver_read_u16(NVS_KEY_AC_TIMER, &u16_val);
    s_ac_state.timer_min = (err == ESP_OK) ? u16_val : s_default_ac_state.timer_min;

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

    nvs_driver_write_u8(NVS_KEY_AC_POWER, state->power ? 1 : 0);
    nvs_driver_write_u8(NVS_KEY_AC_MODE, (uint8_t)state->mode);
    nvs_driver_write_u8(NVS_KEY_AC_TEMP, state->temp);
    nvs_driver_write_u8(NVS_KEY_AC_FAN, (uint8_t)state->fan);
    nvs_driver_write_u8(NVS_KEY_AC_SWING, state->swing ? 1 : 0);
    nvs_driver_write_u8(NVS_KEY_AC_LIGHT, state->light ? 1 : 0);
    nvs_driver_write_u16(NVS_KEY_AC_TIMER, state->timer_min);

    memcpy(&s_ac_state, state, sizeof(ac_state_t));
    return ESP_OK;
}

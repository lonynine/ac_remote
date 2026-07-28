/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "haier_protocol.h"
#include <stdio.h>
#include <string.h>

#define HAIER_HEADER_MARK_US 3080
#define HAIER_HEADER_SPACE1_US 3080
#define HAIER_HEADER_SPACE2_US 4500

#define HAIER_BIT_MARK_US 550
#define HAIER_BIT_0_SPACE_US 530
#define HAIER_BIT_1_SPACE_US 1700

#define HAIER_YRW02_SYMBOL_COUNT (2 + HAIER_YRW02_FRAME_LEN * 8 + 1)

#define HAIER_YRW02_MODEL_A 0xA6
#define HAIER_YRW02_POWER_BIT 0x40
#define HAIER_YRW02_SWING_V_DEFAULT 0x0A
#define HAIER_YRW02_BUTTON_POWER 0x05

static uint8_t haier_yrw02_temp_code(uint8_t temp)
{
  if (temp < 16 || temp > 30) {
    temp = 25;
  }
  return temp - 16;
}

static uint8_t haier_yrw02_mode_code(ac_mode_t mode)
{
  switch (mode) {
  case AC_MODE_AUTO:
    return 0;
  case AC_MODE_COOL:
    return 1;
  case AC_MODE_DRY:
    return 2;
  case AC_MODE_HEAT:
    return 4;
  case AC_MODE_FAN:
    return 6;
  default:
    return 1;
  }
}

static uint8_t haier_yrw02_fan_code(ac_fan_t fan)
{
  switch (fan) {
  case AC_FAN_HIGH:
    return 1;
  case AC_FAN_MED:
    return 2;
  case AC_FAN_LOW:
    return 3;
  case AC_FAN_AUTO:
    return 5;
  default:
    return 3;
  }
}

uint8_t haier_yrw02_checksum(const uint8_t *bytes, size_t len)
{
  uint16_t sum = 0;
  if (!bytes) {
    return 0;
  }

  for (size_t i = 0; i < len; i++) {
    sum += bytes[i];
  }
  return (uint8_t)sum;
}

esp_err_t haier_yrw02_build_frame(const ac_remote_cmd_t *cmd,
                                  uint8_t frame[HAIER_YRW02_FRAME_LEN])
{
  if (!cmd || !frame) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(frame, 0, HAIER_YRW02_FRAME_LEN);

  frame[0] = HAIER_YRW02_MODEL_A;
  // The low nibble stays at the captured default until swing samples are added.
  frame[1] = (haier_yrw02_temp_code(cmd->temp) << 4) | HAIER_YRW02_SWING_V_DEFAULT;
  frame[4] = cmd->power ? HAIER_YRW02_POWER_BIT : 0x00;
  frame[5] = haier_yrw02_fan_code(cmd->fan) << 5;
  frame[7] = haier_yrw02_mode_code(cmd->mode) << 5;
  frame[12] = HAIER_YRW02_BUTTON_POWER;
  frame[13] = haier_yrw02_checksum(frame, HAIER_YRW02_FRAME_LEN - 1);

  return ESP_OK;
}

esp_err_t haier_protocol_encode(const ac_remote_cmd_t *cmd,
                                rmt_symbol_word_t *symbols, size_t symbol_max,
                                size_t *out_count) {
  if (!cmd || !symbols || !out_count || symbol_max < HAIER_YRW02_SYMBOL_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t bytes[HAIER_YRW02_FRAME_LEN];
  esp_err_t err = haier_yrw02_build_frame(cmd, bytes);
  if (err != ESP_OK) {
    return err;
  }

  ir_protocol_print_hex("海尔空调真实物理帧 (精确 115 组脉冲)",
                        bytes, HAIER_YRW02_FRAME_LEN);

  size_t idx = 0;

  /*
      Header

      学习到：
      MARK 3080
      SPACE 3080

      MARK 3080
      SPACE 4500
  */

  symbols[idx].duration0 = HAIER_HEADER_MARK_US;
  symbols[idx].level0 = 1;
  symbols[idx].duration1 = HAIER_HEADER_SPACE1_US;
  symbols[idx].level1 = 0;
  idx++;

  symbols[idx].duration0 = HAIER_HEADER_MARK_US;
  symbols[idx].level0 = 1;
  symbols[idx].duration1 = HAIER_HEADER_SPACE2_US;
  symbols[idx].level1 = 0;
  idx++;

  /*
      连续发送 14 字节
      不再插入中间 Header
  */

  for (int b = 0; b < 14; b++) {
    idx = ir_protocol_append_byte(symbols, idx, bytes[b], HAIER_BIT_MARK_US,
                                  HAIER_BIT_0_SPACE_US, HAIER_BIT_1_SPACE_US);
  }

  /*
      结束

      学习数据:
      最后只有 MARK，没有 SPACE (duration1 = 0)
  */

  symbols[idx].duration0 = HAIER_BIT_MARK_US;
  symbols[idx].level0 = 1;
  symbols[idx].duration1 = 0;
  symbols[idx].level1 = 0;
  idx++;

  *out_count = idx;

  return ESP_OK;
}

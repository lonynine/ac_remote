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

esp_err_t haier_protocol_encode(const ac_remote_cmd_t *cmd,
                                rmt_symbol_word_t *symbols, size_t symbol_max,
                                size_t *out_count) {
  if (!cmd || !symbols || !out_count || symbol_max < 120) {
    return ESP_ERR_INVALID_ARG;
  }

  /*
      海尔真实抓包14字节 (100% 物理复现)

      ON:
      A6 9A 00 00 40 60 00 20 00 00 00 00 05 05

      OFF:
      A6 9A 00 00 00 60 00 20 00 00 00 00 05 C5
  */

  uint8_t bytes[14];

  bytes[0] = 0xA6;
  bytes[1] = 0x9A;
  bytes[2] = 0x00;
  bytes[3] = 0x00;

  if (cmd->power) {
    // 开机
    bytes[4] = 0x40;
    bytes[5] = 0x60;
    bytes[13] = 0x05;
  } else {
    // 关机
    bytes[4] = 0x00;
    bytes[5] = 0x60;
    bytes[13] = 0xC5;
  }

  bytes[6] = 0x00;
  bytes[7] = 0x20;
  bytes[8] = 0x00;
  bytes[9] = 0x00;
  bytes[10] = 0x00;
  bytes[11] = 0x00;

  bytes[12] = 0x05;

  ir_protocol_print_hex("海尔空调真实物理帧 (精确 115 组脉冲)", bytes, 14);

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

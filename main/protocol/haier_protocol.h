/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAIER_PROTOCOL_H
#define HAIER_PROTOCOL_H

#include "ir_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAIER_YRW02_FRAME_LEN 14

/**
 * @brief Build a Haier YRW02 14-byte state frame.
 */
esp_err_t haier_yrw02_build_frame(const ac_remote_cmd_t *cmd,
                                  uint8_t frame[HAIER_YRW02_FRAME_LEN]);

/**
 * @brief Calculate the low 8 bits of the byte sum.
 */
uint8_t haier_yrw02_checksum(const uint8_t *bytes, size_t len);

/**
 * @brief 海尔空调 (Haier YRW02 / 05) 纯 C 协议组帧算法
 * @param cmd 空调控制状态参数
 * @param symbols 输出的 RMT 脉冲符号数组
 * @param symbol_max 符号数组最大容纳量
 * @param out_count 输出实际产生的符号数量
 */
esp_err_t haier_protocol_encode(const ac_remote_cmd_t *cmd, 
                                rmt_symbol_word_t *symbols, 
                                size_t symbol_max, 
                                size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif // HAIER_PROTOCOL_H

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ir_remote.h"
#include "ir_driver.h"
#include "haier_protocol.h"
#include "gree_protocol.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ir_remote";

// 学码缓冲区 (最大扩充至 256 组波形脉冲)
#define LEARN_BUF_MAX 256
static rmt_symbol_word_t s_learned_symbols[LEARN_BUF_MAX];
static size_t s_learned_count = 0;

esp_err_t ir_remote_init(void)
{
    // 1. 初始化底层 RMT 发送总线 (IO4, 38kHz 载波)
    esp_err_t err = ir_driver_tx_init(IR_TX_GPIO_NUM, 38000);
    if (err != ESP_OK) return err;

    // 2. 初始化底层 RMT 接收总线 (IO5)
    err = ir_driver_rx_init(IR_RX_GPIO_NUM);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "红外遥控硬件设备初始化成功! (发送: IO%d, 接收: IO%d)", IR_TX_GPIO_NUM, IR_RX_GPIO_NUM);
    }

    return err;
}

static void print_sent_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    printf("\n================ [ 纯 C 协议编码发波原始数据 (%zu 组) ] ================\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  [%02zu] 高电平 (MARK): %5u us  |  低电平 (SPACE): %5u us\n",
               i, symbols[i].duration0, symbols[i].duration1);
    }
    printf("========================================================================\n\n");
}

esp_err_t ir_remote_send_cmd(const ac_remote_cmd_t *cmd)
{
    if (!cmd) return ESP_ERR_INVALID_ARG;

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
        ESP_LOGE(TAG, "品牌 [%d] 红外协议打包失败: %s", cmd->brand, esp_err_to_name(err));
        return err;
    }

    // 触发底层硬件 RMT 脉冲发送
    err = ir_driver_tx_symbols(symbols, symbol_count);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "已成功通过纯 C 语言协议层发送品牌 [%d] 红外指令 [%s | 模式:%d | 温度:%d℃ | 风速:%d]",
                 cmd->brand, cmd->power ? "开机" : "关机", cmd->mode, cmd->temp, cmd->fan);
        
        // 打印原始发送的 RMT 波形列表
        print_sent_symbols(symbols, symbol_count);
    } else {
        ESP_LOGE(TAG, "发送红外脉冲失败: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t ir_remote_send_haier(const haier_ac_status_t *status)
{
    ac_remote_cmd_t cmd = *status;
    cmd.brand = AC_BRAND_HAIER;
    return ir_remote_send_cmd(&cmd);
}

esp_err_t ir_remote_send_gree(const gree_ac_status_t *status)
{
    ac_remote_cmd_t cmd = *status;
    cmd.brand = AC_BRAND_GREE;
    return ir_remote_send_cmd(&cmd);
}

// 捕获脉冲转译 16 进制 Hex 字节解调器
static void decode_and_print_hex(const rmt_symbol_word_t *symbols, size_t count)
{
    if (count < 10) return;

    // 寻找数据起始点 (跳过 >2000us 的引导头)
    size_t start_idx = 0;
    while (start_idx < count && (symbols[start_idx].duration0 > 2000 || symbols[start_idx].duration1 > 2000)) {
        start_idx++;
    }

    if (start_idx >= count) return;

    uint8_t current_byte = 0;
    int bit_count = 0;
    uint8_t hex_bytes[64];
    size_t byte_count = 0;

    for (size_t i = start_idx; i < count; i++) {
        if (symbols[i].duration0 == 0) break;

        bool bit = (symbols[i].duration1 > 1000); // 低电平大于 1000us 判定为 1，否则为 0
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

    if (byte_count > 0) {
        printf("📦 [学码 Hex 字节解调 (%zu 字节)]: ", byte_count);
        for (size_t b = 0; b < byte_count; b++) {
            printf("0x%02X ", hex_bytes[b]);
        }
        printf("\n");
    }
}

static void print_raw_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    // 1. 先解调并输出 16 进制 Hex 字节
    decode_and_print_hex(symbols, count);

    // 2. 输出微秒级脉冲波形列表
    printf("\n================ [ 捕获到的红外原始波形数据 (%zu 组) ] ================\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  [%02zu] 高电平 (MARK): %5u us  |  低电平 (SPACE): %5u us\n",
               i, symbols[i].duration0, symbols[i].duration1);
    }
    printf("========================================================================\n\n");
}

esp_err_t ir_remote_learn_start(uint32_t timeout_sec)
{
    if (timeout_sec == 0) timeout_sec = 10; // 默认延长为 10 秒连续抓包

    printf("\n========================================================================\n");
    printf("📡 [红外遥控 10 秒不间断连续抓包模式开启]\n");
    printf("   请在 10 秒内连续对准接收头 (IO5) 按下遥控器按键...\n");
    printf("   系统将实时捕获并毫无保留地打印每一次按键的全量数据！\n");
    printf("========================================================================\n\n");

    uint32_t total_wait_ms = timeout_sec * 1000;
    uint32_t elapsed_ms = 0;
    int capture_index = 1;

    while (elapsed_ms < total_wait_ms) {
        memset(s_learned_symbols, 0, sizeof(s_learned_symbols));
        s_learned_count = 0;

        // 启动一次物理 RMT 接收
        ir_driver_rx_receive(s_learned_symbols, LEARN_BUF_MAX, &s_learned_count, 1000);

        // 轮询等待 1 秒内是否有信号产生
        for (int check = 0; check < 10; check++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            elapsed_ms += 100;

            if (s_learned_symbols[0].duration0 > 0) {
                // 计算捕获到的真实脉冲组数
                size_t count = 0;
                for (size_t i = 0; i < LEARN_BUF_MAX; i++) {
                    if (s_learned_symbols[i].duration0 > 0) count++;
                    else break;
                }
                s_learned_count = count;

                printf("🎉 [第 %d 次按键捕获成功] 脉冲组数: %zu\n", capture_index++, s_learned_count);
                print_raw_symbols(s_learned_symbols, s_learned_count);
                break;
            }
        }
    }

    printf("⏱️ [10 秒抓包时间结束] 红外接收已关闭。最后一组捕获数据保存在缓存中，可用 'ac emit' 重发。\n\n");
    return ESP_OK;
}

esp_err_t ir_remote_learn_emit(void)
{
    if (s_learned_count == 0 || s_learned_symbols[0].duration0 == 0) {
        printf("错误: 当前尚未捕获任何学码数据！请先执行 'ac learn' 进行红外学习。\n");
        return ESP_ERR_INVALID_STATE;
    }

    // 关键电平极性校准：确保 RMT 发送时高电平开启 38kHz 载波 (NMOS 导通发光)
    rmt_symbol_word_t tx_symbols[LEARN_BUF_MAX];
    for (size_t i = 0; i < s_learned_count; i++) {
        tx_symbols[i].duration0 = s_learned_symbols[i].duration0;
        tx_symbols[i].level0 = 1; // MARK 载波开启
        tx_symbols[i].duration1 = s_learned_symbols[i].duration1;
        tx_symbols[i].level1 = 0; // SPACE 载波关闭
    }

    printf("正在重发刚刚学到的 %zu 组红外原始波形 (已完成 TX 电平极性校准)...\n", s_learned_count);
    esp_err_t err = ir_driver_tx_symbols(tx_symbols, s_learned_count);
    if (err == ESP_OK) {
        printf("已成功重发学到的红外波形！\n");
    } else {
        printf("重发红外波形失败: %s\n", esp_err_to_name(err));
    }
    return err;
}

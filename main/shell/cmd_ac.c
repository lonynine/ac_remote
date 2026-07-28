/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_ac.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "ac_state.h"
#include "control_task.h"
#include "ir_remote.h"

static bool is_number(const char *str)
{
    if (!str || *str == '\0') return false;
    while (*str) {
        if (!isdigit((unsigned char)*str)) return false;
        str++;
    }
    return true;
}

static int do_cmd_ac(int argc, char **argv)
{
    if (argc < 2) {
        printf("\n使用方法: ac <on|off|set|send|learn|emit> [brand] [power] [mode] [temp] [fan]\n");
        printf("  示例: ac on                           (开启海尔空调, 默认 25℃ 制冷)\n");
        printf("  示例: ac off                          (关闭海尔空调)\n");
        printf("  示例: ac send haier on cool 25 low    (推入队列发送海尔 25℃ 制冷低风开机)\n");
        printf("  示例: ac send gree off                (推入队列发送格力关机指令)\n");
        printf("  示例: ac learn 5                      (开启 5 秒红外学码)\n");
        printf("  示例: ac emit                         (推入队列重发学到的红外信号)\n\n");
        return 0;
    }

    const char *action = argv[1];

    if (strcasecmp(action, "learn") == 0) {
        uint32_t timeout = 5;
        if (argc >= 3 && is_number(argv[2])) {
            timeout = (uint32_t)atoi(argv[2]);
        }
        printf("开始红外学习，等待 %lu 秒...\n", (unsigned long)timeout);
        esp_err_t err = ir_remote_learn_start(timeout);
        if (err != ESP_OK) {
            printf("红外学习失败: %s\n", esp_err_to_name(err));
            return err;
        }
        printf("红外学习结束，可使用 ac emit 重发最后一次捕获的波形。\n");
        return ESP_OK;
    }

    if (strcasecmp(action, "emit") == 0) {
        // 唯一的硬件管控入口：推入队列
        esp_err_t err = control_task_post_emit();
        if (err != ESP_OK) {
            printf("推入 'ac emit' 重发指令失败: %s\n", esp_err_to_name(err));
            return err;
        }
        printf("已将 'ac emit' 重发指令推入独占控制队列...\n");
        return ESP_OK;
    }

    // 默认指令参数构造
    ac_remote_cmd_t cmd = {0};
    cmd.brand = AC_BRAND_HAIER;
    cmd.power = true;
    cmd.mode = AC_MODE_COOL;
    cmd.temp = 25;
    cmd.fan = AC_FAN_LOW;
    cmd.swing = false;
    cmd.light = false;

    if (strcasecmp(action, "off") == 0) {
        cmd.power = false;
    }

    // 智能独立分类扫描 Token
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        // 1. 匹配开关
        if (strcasecmp(arg, "on") == 0) {
            cmd.power = true;
        } else if (strcasecmp(arg, "off") == 0) {
            cmd.power = false;
        }

        // 2. 匹配品牌
        if (strcasecmp(arg, "haier") == 0) {
            cmd.brand = AC_BRAND_HAIER;
        } else if (strcasecmp(arg, "gree") == 0) {
            cmd.brand = AC_BRAND_GREE;
        } else if (strcasecmp(arg, "midea") == 0) {
            cmd.brand = AC_BRAND_MIDEA;
        } else if (strcasecmp(arg, "aux") == 0) {
            cmd.brand = AC_BRAND_AUX;
        }

        // 3. 匹配模式
        if (strcasecmp(arg, "cool") == 0) {
            cmd.mode = AC_MODE_COOL;
        } else if (strcasecmp(arg, "heat") == 0) {
            cmd.mode = AC_MODE_HEAT;
        } else if (strcasecmp(arg, "dry") == 0) {
            cmd.mode = AC_MODE_DRY;
        } else if (strcasecmp(arg, "fan") == 0) {
            cmd.mode = AC_MODE_FAN;
        }

        // 4. 匹配风速
        if (strcasecmp(arg, "low") == 0) {
            cmd.fan = AC_FAN_LOW;
        } else if (strcasecmp(arg, "med") == 0 || strcasecmp(arg, "medium") == 0) {
            cmd.fan = AC_FAN_MED;
        } else if (strcasecmp(arg, "high") == 0) {
            cmd.fan = AC_FAN_HIGH;
        } else if (strcasecmp(arg, "autofan") == 0) {
            cmd.fan = AC_FAN_AUTO;
        }

        // 5. 匹配温度 (纯数字 16 ~ 30)
        if (is_number(arg)) {
            int t = atoi(arg);
            if (t >= 16 && t <= 30) {
                cmd.temp = (uint8_t)t;
            }
        }
    }

    // 唯一的硬件管控入口：推入队列
    esp_err_t err = control_task_post_cmd(&cmd);
    if (err != ESP_OK) {
        printf("推入空调控制指令失败: %s\n", esp_err_to_name(err));
        return err;
    }
    printf("已将空调控制指令推入 FreeRTOS 独占队列 (品牌:%d | %s | %d℃ | 模式:%d)...\n",
           cmd.brand, cmd.power ? "开机" : "关机", cmd.temp, cmd.mode);

    return ESP_OK;
}

esp_err_t register_cmd_ac(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ac",
        .help = "红外空调控制与学习命令 (示例: ac send haier on cool 25 low / ac learn / ac emit)",
        .hint = NULL,
        .func = &do_cmd_ac,
        .argtable = NULL
    };

    return esp_console_cmd_register(&cmd);
}

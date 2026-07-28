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
#include "protocol_manager.h"

static bool is_number(const char *str)
{
    if (!str || *str == '\0') return false;
    while (*str) {
        if (!isdigit((unsigned char)*str)) return false;
        str++;
    }
    return true;
}

static bool parse_brand(const char *name, ac_brand_t *brand)
{
    if (!name || !brand) return false;

    if (strcasecmp(name, "haier") == 0) {
        *brand = AC_BRAND_HAIER;
    } else if (strcasecmp(name, "gree") == 0) {
        *brand = AC_BRAND_GREE;
    } else if (strcasecmp(name, "midea") == 0) {
        *brand = AC_BRAND_MIDEA;
    } else if (strcasecmp(name, "aux") == 0) {
        *brand = AC_BRAND_AUX;
    } else {
        return false;
    }
    return true;
}

static void print_ac_help(void)
{
    printf("\n空调红外控制命令:\n");
    printf("  ac get                                      查看最后发送的缓存状态\n");
    printf("  ac on                                       海尔空调开机，默认 25℃ 制冷低风\n");
    printf("  ac off                                      海尔空调关机\n");
    printf("  ac send <brand> <on|off> [mode] [temp] [fan]\n");
    printf("      brand: haier | gree\n");
    printf("      mode : auto | cool | dry | fan | heat\n");
    printf("      temp : 16 ~ 30\n");
    printf("      fan  : autofan | low | med | high\n");
    printf("  ac timer <brand> on <minutes>               空调自身定时开机\n");
    printf("  ac timer <brand> off <minutes>              空调自身定时关机\n");
    printf("  ac timer <brand> cancel                     取消空调自身定时\n");
    printf("      当前仅 haier 支持协议定时，范围 1 ~ 1439 分钟\n");
    printf("  ac learn [seconds]                          学习红外波形，默认 5 秒\n");
    printf("  ac emit                                     重发最后一次学习的波形\n");
    printf("  ac help                                     显示本帮助\n");
    printf("\n示例:\n");
    printf("  ac send haier on cool 25 low\n");
    printf("  ac timer haier on 60\n");
    printf("  ac timer haier off 90\n");
    printf("  ac timer haier cancel\n\n");
}

static const char *timer_mode_name(ac_timer_mode_t mode)
{
    switch (mode) {
    case AC_TIMER_OFF: return "定时关机";
    case AC_TIMER_ON: return "定时开机";
    case AC_TIMER_ON_THEN_OFF: return "先开后关";
    case AC_TIMER_OFF_THEN_ON: return "先关后开";
    default: return "关闭";
    }
}

static void print_ac_state(void)
{
    ac_state_t state;
    esp_err_t err = ac_state_get(&state);
    if (err != ESP_OK) {
        printf("读取空调状态失败: %s\n", esp_err_to_name(err));
        return;
    }

    printf("品牌:%s 电源:%s 模式:%d 温度:%u℃ 风速:%d 摆风:%s 灯光:%s\n",
           ac_protocol_brand_name(state.brand), state.power ? "开" : "关",
           state.mode, state.temp, state.fan,
           state.swing ? "开" : "关", state.light ? "开" : "关");
    printf("协议定时:%s 开机:%u分钟 关机:%u分钟\n",
           timer_mode_name(state.timer_mode), state.on_timer_min, state.off_timer_min);
}

static esp_err_t fill_request_from_state(ac_request_t *request)
{
    ac_state_t state;
    esp_err_t err = ac_state_get(&state);
    if (err != ESP_OK) {
        return err;
    }

    memset(request, 0, sizeof(*request));
    request->brand = state.brand;
    request->action = AC_ACTION_SET_STATE;
    request->power = state.power;
    request->mode = state.mode;
    request->temp = state.temp;
    request->fan = state.fan;
    request->swing = state.swing;
    request->light = state.light;
    return ESP_OK;
}

static int do_cmd_ac(int argc, char **argv)
{
    if (argc < 2) {
        print_ac_help();
        return 0;
    }

    const char *action = argv[1];

    if (strcasecmp(action, "help") == 0) {
        print_ac_help();
        return ESP_OK;
    }

    if (strcasecmp(action, "get") == 0) {
        print_ac_state();
        return ESP_OK;
    }

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

    ac_request_t request;
    esp_err_t err = fill_request_from_state(&request);
    if (err != ESP_OK) {
        printf("读取空调状态失败: %s\n", esp_err_to_name(err));
        return err;
    }

    if (strcasecmp(action, "timer") == 0) {
        if (argc < 4) {
            printf("用法: ac timer <brand> <on|off|cancel> [minutes]\n");
            return ESP_ERR_INVALID_ARG;
        }

        if (!parse_brand(argv[2], &request.brand)) {
            printf("未知空调品牌: %s\n", argv[2]);
            return ESP_ERR_INVALID_ARG;
        }
        const char *timer_action = argv[3];
        if (strcasecmp(timer_action, "on") == 0) {
            request.action = AC_ACTION_TIMER_ON;
        } else if (strcasecmp(timer_action, "off") == 0) {
            request.action = AC_ACTION_TIMER_OFF;
        } else if (strcasecmp(timer_action, "cancel") == 0) {
            request.action = AC_ACTION_TIMER_CANCEL;
        } else {
            printf("定时操作仅支持 on、off 或 cancel\n");
            return ESP_ERR_INVALID_ARG;
        }
        if (!ac_protocol_supports(request.brand, request.action)) {
            printf("品牌 %s 暂不支持红外协议定时\n", argv[2]);
            return ESP_ERR_NOT_SUPPORTED;
        }

        if (strcasecmp(timer_action, "cancel") == 0) {
            if (argc != 4) {
                printf("用法: ac timer <brand> cancel\n");
                return ESP_ERR_INVALID_ARG;
            }
            request.timer_minutes = 0;
        } else {
            if (argc != 5 || !is_number(argv[4])) {
                printf("用法: ac timer <brand> <on|off> <minutes>\n");
                return ESP_ERR_INVALID_ARG;
            }
            unsigned long minutes = strtoul(argv[4], NULL, 10);
            if (minutes == 0 || minutes > 1439) {
                printf("定时时间必须为 1 ~ 1439 分钟\n");
                return ESP_ERR_INVALID_ARG;
            }
            request.timer_minutes = (uint16_t)minutes;
        }

        err = control_task_post_request(&request);
        if (err != ESP_OK) {
            printf("推入空调定时指令失败: %s\n", esp_err_to_name(err));
        }
        return err;
    }

    if (strcasecmp(action, "set") == 0) {
        printf("ac set 已删除，请使用 ac send\n");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (strcasecmp(action, "on") != 0 && strcasecmp(action, "off") != 0 &&
        strcasecmp(action, "send") != 0) {
        printf("未知 ac 子命令: %s，请使用 'ac help' 查看帮助\n", action);
        return ESP_ERR_INVALID_ARG;
    }

    if (strcasecmp(action, "send") == 0) {
        if (argc < 4) {
            printf("用法: ac send <haier|gree> <on|off> [mode] [temp] [fan]\n");
            return ESP_ERR_INVALID_ARG;
        }
        if (!parse_brand(argv[2], &request.brand) ||
            !ac_protocol_supports(request.brand, AC_ACTION_SET_STATE)) {
            printf("品牌 %s 暂不支持红外发送\n", argv[2]);
            return ESP_ERR_NOT_SUPPORTED;
        }
        if (strcasecmp(argv[3], "on") == 0) {
            request.power = true;
        } else if (strcasecmp(argv[3], "off") == 0) {
            request.power = false;
        } else {
            printf("电源参数必须为 on 或 off\n");
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (strcasecmp(action, "on") == 0) {
        request.brand = AC_BRAND_HAIER;
        request.power = true;
        request.mode = AC_MODE_COOL;
        request.temp = 25;
        request.fan = AC_FAN_LOW;
    }

    if (strcasecmp(action, "off") == 0) {
        request.brand = AC_BRAND_HAIER;
        request.power = false;
    }

    // 智能独立分类扫描 Token
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        // 1. 匹配开关
        if (strcasecmp(arg, "on") == 0) {
            request.power = true;
        } else if (strcasecmp(arg, "off") == 0) {
            request.power = false;
        }

        // 2. 匹配模式
        if (strcasecmp(arg, "auto") == 0) {
            request.mode = AC_MODE_AUTO;
        } else if (strcasecmp(arg, "cool") == 0) {
            request.mode = AC_MODE_COOL;
        } else if (strcasecmp(arg, "heat") == 0) {
            request.mode = AC_MODE_HEAT;
        } else if (strcasecmp(arg, "dry") == 0) {
            request.mode = AC_MODE_DRY;
        } else if (strcasecmp(arg, "fan") == 0) {
            request.mode = AC_MODE_FAN;
        }

        // 3. 匹配风速
        if (strcasecmp(arg, "low") == 0) {
            request.fan = AC_FAN_LOW;
        } else if (strcasecmp(arg, "med") == 0 || strcasecmp(arg, "medium") == 0) {
            request.fan = AC_FAN_MED;
        } else if (strcasecmp(arg, "high") == 0) {
            request.fan = AC_FAN_HIGH;
        } else if (strcasecmp(arg, "autofan") == 0) {
            request.fan = AC_FAN_AUTO;
        }

        // 4. 匹配温度 (纯数字 16 ~ 30)
        if (is_number(arg)) {
            int t = atoi(arg);
            if (t >= 16 && t <= 30) {
                request.temp = (uint8_t)t;
            }
        }
    }

    // 唯一的硬件管控入口：推入队列
    err = control_task_post_request(&request);
    if (err != ESP_OK) {
        printf("推入空调控制指令失败: %s\n", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t register_cmd_ac(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ac",
        .help = "空调红外控制命令\n"
                "  ac get\n"
                "  ac on | ac off\n"
                "  ac send <brand> <on|off> [mode] [temp] [fan]\n"
                "    brand: haier | gree\n"
                "    mode: auto | cool | dry | fan | heat\n"
                "    temp: 16-30\n"
                "    fan: autofan | low | med | high\n"
                "  ac timer <brand> <on|off> <minutes>  (当前仅 haier)\n"
                "  ac timer <brand> cancel              (当前仅 haier)\n"
                "  ac learn [seconds]\n"
                "  ac emit\n"
                "  ac help",
        .hint = NULL,
        .func = &do_cmd_ac,
        .argtable = NULL
    };

    return esp_console_cmd_register(&cmd);
}

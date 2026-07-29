/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_ac.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ac_state.h"
#include "argtable3/argtable3.h"
#include "control_task.h"
#include "esp_console.h"
#include "ir_remote.h"
#include "protocol_manager.h"

static struct {
    struct arg_str *action;
    struct arg_str *params;
    struct arg_end *end;
} ac_args;

static bool parse_brand(const char *value, ac_brand_t *brand)
{
    if (strcasecmp(value, "haier") == 0) {
        *brand = AC_BRAND_HAIER;
    } else if (strcasecmp(value, "gree") == 0) {
        *brand = AC_BRAND_GREE;
    } else {
        return false;
    }
    return true;
}

static bool parse_power(const char *value, bool *power)
{
    if (strcasecmp(value, "on") == 0) {
        *power = true;
    } else if (strcasecmp(value, "off") == 0) {
        *power = false;
    } else {
        return false;
    }
    return true;
}

static bool parse_mode(const char *value, ac_mode_t *mode)
{
    if (strcasecmp(value, "auto") == 0) {
        *mode = AC_MODE_AUTO;
    } else if (strcasecmp(value, "cool") == 0) {
        *mode = AC_MODE_COOL;
    } else if (strcasecmp(value, "dry") == 0) {
        *mode = AC_MODE_DRY;
    } else if (strcasecmp(value, "fan") == 0) {
        *mode = AC_MODE_FAN;
    } else if (strcasecmp(value, "heat") == 0) {
        *mode = AC_MODE_HEAT;
    } else {
        return false;
    }
    return true;
}

static bool parse_fan(const char *value, ac_fan_t *fan)
{
    if (strcasecmp(value, "autofan") == 0 ||
        strcasecmp(value, "auto") == 0) {
        *fan = AC_FAN_AUTO;
    } else if (strcasecmp(value, "low") == 0) {
        *fan = AC_FAN_LOW;
    } else if (strcasecmp(value, "med") == 0 ||
               strcasecmp(value, "medium") == 0) {
        *fan = AC_FAN_MED;
    } else if (strcasecmp(value, "high") == 0) {
        *fan = AC_FAN_HIGH;
    } else {
        return false;
    }
    return true;
}

static esp_err_t parse_uint_range(const char *value, unsigned long minimum,
                                  unsigned long maximum,
                                  unsigned long *result)
{
    if (!value || value[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        parsed < minimum || parsed > maximum) {
        return ESP_ERR_INVALID_ARG;
    }
    *result = parsed;
    return ESP_OK;
}

static const char *mode_name(ac_mode_t mode)
{
    static const char *names[] = {"auto", "cool", "dry", "fan", "heat"};
    return mode <= AC_MODE_HEAT ? names[mode] : "unknown";
}

static const char *fan_name(ac_fan_t fan)
{
    static const char *names[] = {"autofan", "low", "med", "high"};
    return fan <= AC_FAN_HIGH ? names[fan] : "unknown";
}

static const char *timer_name(ac_timer_mode_t timer)
{
    switch (timer) {
    case AC_TIMER_OFF: return "off";
    case AC_TIMER_ON: return "on";
    case AC_TIMER_ON_THEN_OFF: return "on-then-off";
    case AC_TIMER_OFF_THEN_ON: return "off-then-on";
    default: return "none";
    }
}

static esp_err_t print_ac_state(void)
{
    ac_state_t state;
    esp_err_t err = ac_state_get(&state);
    if (err != ESP_OK) {
        printf("读取空调状态失败: %s\n", esp_err_to_name(err));
        return err;
    }

    printf("brand: %s\n", ac_protocol_brand_name(state.brand));
    printf("power: %s\n", state.power ? "on" : "off");
    printf("mode: %s\n", mode_name(state.mode));
    printf("temperature: %u\n", state.temp);
    printf("fan: %s\n", fan_name(state.fan));
    printf("swing: %s\n", state.swing ? "on" : "off");
    printf("light: %s\n", state.light ? "on" : "off");
    printf("timer.mode: %s\n", timer_name(state.timer_mode));
    printf("timer.on_minutes: %u\n", state.on_timer_min);
    printf("timer.off_minutes: %u\n", state.off_timer_min);
    return ESP_OK;
}

static esp_err_t request_from_state(ac_request_t *request)
{
    ac_state_t state;
    esp_err_t err = ac_state_get(&state);
    if (err != ESP_OK) {
        return err;
    }
    *request = (ac_request_t) {
        .brand = state.brand,
        .action = AC_ACTION_SET_STATE,
        .power = state.power,
        .mode = state.mode,
        .temp = state.temp,
        .fan = state.fan,
        .swing = state.swing,
        .light = state.light,
    };
    return ESP_OK;
}

static esp_err_t require_param_count(const char *action, int actual,
                                     int minimum, int maximum)
{
    if (actual >= minimum && actual <= maximum) {
        return ESP_OK;
    }
    printf("ac %s 参数数量错误，请使用 'help ac' 查看用法\n", action);
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t handle_send(const char **params, int count)
{
    esp_err_t err = require_param_count("send", count, 2, 5);
    if (err != ESP_OK) {
        return err;
    }

    ac_request_t request;
    err = request_from_state(&request);
    if (err != ESP_OK) {
        return err;
    }
    request.action = AC_ACTION_SET_STATE;
    if (!parse_brand(params[0], &request.brand) ||
        !ac_protocol_supports(request.brand, request.action)) {
        printf("不支持的空调品牌: %s\n", params[0]);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!parse_power(params[1], &request.power)) {
        printf("power 必须为 on 或 off\n");
        return ESP_ERR_INVALID_ARG;
    }
    if (count >= 3 && !parse_mode(params[2], &request.mode)) {
        printf("mode 必须为 auto、cool、dry、fan 或 heat\n");
        return ESP_ERR_INVALID_ARG;
    }
    if (count >= 4) {
        unsigned long temperature;
        if (parse_uint_range(params[3], 16, 30, &temperature) != ESP_OK) {
            printf("temperature 范围必须为 16 到 30\n");
            return ESP_ERR_INVALID_ARG;
        }
        request.temp = (uint8_t)temperature;
    }
    if (count >= 5 && !parse_fan(params[4], &request.fan)) {
        printf("fan 必须为 autofan、low、med 或 high\n");
        return ESP_ERR_INVALID_ARG;
    }
    return control_task_post_request(&request);
}

static esp_err_t handle_timer(const char **params, int count)
{
    esp_err_t err = require_param_count("timer", count, 2, 3);
    if (err != ESP_OK) {
        return err;
    }

    ac_request_t request;
    err = request_from_state(&request);
    if (err != ESP_OK) {
        return err;
    }
    if (!parse_brand(params[0], &request.brand)) {
        printf("未知空调品牌: %s\n", params[0]);
        return ESP_ERR_INVALID_ARG;
    }

    if (strcasecmp(params[1], "on") == 0) {
        request.action = AC_ACTION_TIMER_ON;
    } else if (strcasecmp(params[1], "off") == 0) {
        request.action = AC_ACTION_TIMER_OFF;
    } else if (strcasecmp(params[1], "cancel") == 0) {
        request.action = AC_ACTION_TIMER_CANCEL;
    } else {
        printf("timer action 必须为 on、off 或 cancel\n");
        return ESP_ERR_INVALID_ARG;
    }
    if (!ac_protocol_supports(request.brand, request.action)) {
        printf("品牌 %s 不支持该协议定时动作\n", params[0]);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (request.action == AC_ACTION_TIMER_CANCEL) {
        if (count != 2) {
            printf("取消定时不接受 minutes 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        if (count != 3) {
            printf("定时开关需要 minutes 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        unsigned long minutes;
        if (parse_uint_range(params[2], 1, 1439, &minutes) != ESP_OK) {
            printf("minutes 范围必须为 1 到 1439\n");
            return ESP_ERR_INVALID_ARG;
        }
        request.timer_minutes = (uint16_t)minutes;
    }
    return control_task_post_request(&request);
}

static int do_cmd_ac(int argc, char **argv)
{
    int errors = arg_parse(argc, argv, (void **)&ac_args);
    if (errors != 0) {
        arg_print_errors(stderr, ac_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    const char *action = ac_args.action->sval[0];
    const char **params = ac_args.params->sval;
    int count = ac_args.params->count;

    if (strcasecmp(action, "get") == 0) {
        if (require_param_count(action, count, 0, 0) != ESP_OK) {
            return ESP_ERR_INVALID_ARG;
        }
        return print_ac_state();
    }
    if (strcasecmp(action, "send") == 0) {
        return handle_send(params, count);
    }
    if (strcasecmp(action, "timer") == 0) {
        return handle_timer(params, count);
    }
    if (strcasecmp(action, "on") == 0 || strcasecmp(action, "off") == 0) {
        if (require_param_count(action, count, 0, 0) != ESP_OK) {
            return ESP_ERR_INVALID_ARG;
        }
        if (strcasecmp(action, "on") == 0) {
            const char *quick_params[] = {"haier", "on", "cool", "25", "low"};
            return handle_send(quick_params, 5);
        }
        const char *quick_params[] = {"haier", "off"};
        return handle_send(quick_params, 2);
    }
    if (strcasecmp(action, "learn") == 0) {
        if (require_param_count(action, count, 0, 1) != ESP_OK) {
            return ESP_ERR_INVALID_ARG;
        }
        unsigned long seconds = 5;
        if (count == 1 &&
            parse_uint_range(params[0], 1, UINT32_MAX, &seconds) != ESP_OK) {
            printf("seconds 必须为大于 0 的整数\n");
            return ESP_ERR_INVALID_ARG;
        }
        return ir_remote_learn_start((uint32_t)seconds);
    }
    if (strcasecmp(action, "emit") == 0) {
        if (require_param_count(action, count, 0, 0) != ESP_OK) {
            return ESP_ERR_INVALID_ARG;
        }
        return control_task_post_emit();
    }

    printf("未知操作: %s，可用操作: get | on | off | send | timer | learn | emit\n",
           action);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t register_cmd_ac(void)
{
    ac_args.action = arg_str1(
        NULL, NULL, "<get|on|off|send|timer|learn|emit>",
        "操作: 查询、快捷开关、发送状态、协议定时、红外学习或重发");
    ac_args.params = arg_strn(
        NULL, NULL, "<param>", 0, 5,
        "send: <brand> <on|off> [mode] [temperature] [fan]; "
        "timer: <brand> <on|off|cancel> [minutes]; learn: [seconds]");
    ac_args.end = arg_end(6);

    const esp_console_cmd_t command = {
        .command = "ac",
        .help = "空调红外控制（brand: haier|gree，temperature: 16-30，timer: 1-1439 分钟）",
        .hint = NULL,
        .func = &do_cmd_ac,
        .argtable = &ac_args,
    };
    return esp_console_cmd_register(&command);
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

static struct {
    struct arg_str *action;
    struct arg_str *key;
    struct arg_str *value;
    struct arg_end *end;
} config_args;

typedef enum {
    CONFIG_FIELD_WIFI_SSID,
    CONFIG_FIELD_WIFI_PASSWORD,
    CONFIG_FIELD_DEVICE_NAME,
    CONFIG_FIELD_DEVICE_ID,
    CONFIG_FIELD_MDNS_HOSTNAME,
    CONFIG_FIELD_UNKNOWN,
} config_field_t;

static config_field_t config_field_from_name(const char *name)
{
    if (!name) {
        return CONFIG_FIELD_UNKNOWN;
    }
    if (strcasecmp(name, "wifi.ssid") == 0 ||
        strcasecmp(name, "wifi_ssid") == 0) {
        return CONFIG_FIELD_WIFI_SSID;
    }
    if (strcasecmp(name, "wifi.password") == 0 ||
        strcasecmp(name, "wifi_password") == 0) {
        return CONFIG_FIELD_WIFI_PASSWORD;
    }
    if (strcasecmp(name, "device.name") == 0 ||
        strcasecmp(name, "device_name") == 0) {
        return CONFIG_FIELD_DEVICE_NAME;
    }
    if (strcasecmp(name, "device.id") == 0 ||
        strcasecmp(name, "device_id") == 0) {
        return CONFIG_FIELD_DEVICE_ID;
    }
    if (strcasecmp(name, "mdns.hostname") == 0 ||
        strcasecmp(name, "mdns_hostname") == 0) {
        return CONFIG_FIELD_MDNS_HOSTNAME;
    }
    return CONFIG_FIELD_UNKNOWN;
}

static void print_config_field(const sys_config_t *config,
                               config_field_t field)
{
    switch (field) {
    case CONFIG_FIELD_WIFI_SSID:
        printf("wifi.ssid: %s\n",
               config->wifi_ssid[0] ? config->wifi_ssid : "(未配置)");
        break;
    case CONFIG_FIELD_WIFI_PASSWORD:
        printf("wifi.password: %s\n",
               config->wifi_password[0]
                   ? config->wifi_password : "(未配置)");
        break;
    case CONFIG_FIELD_DEVICE_NAME:
        printf("device.name: %s\n", config->device_name);
        break;
    case CONFIG_FIELD_DEVICE_ID:
        printf("device.id: %u\n", config->device_id);
        break;
    case CONFIG_FIELD_MDNS_HOSTNAME:
        if (config->mdns_hostname[0]) {
            printf("mdns.hostname: %s\n", config->mdns_hostname);
        } else {
            printf("mdns.hostname: (自动生成: ac-remote-%u)\n",
                   config->device_id);
        }
        break;
    default:
        break;
    }
}

static void print_all_config(const sys_config_t *config)
{
    print_config_field(config, CONFIG_FIELD_WIFI_SSID);
    print_config_field(config, CONFIG_FIELD_WIFI_PASSWORD);
    print_config_field(config, CONFIG_FIELD_DEVICE_NAME);
    print_config_field(config, CONFIG_FIELD_DEVICE_ID);
    print_config_field(config, CONFIG_FIELD_MDNS_HOSTNAME);
}

static esp_err_t parse_device_id(const char *value, uint16_t *device_id)
{
    if (!value || !device_id || value[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    *device_id = (uint16_t)parsed;
    return ESP_OK;
}

static esp_err_t set_string(char *destination, size_t capacity,
                            const char *value, const char *field_name)
{
    size_t length = strlen(value);
    if (length >= capacity) {
        printf("%s 过长，最多允许 %u 个字符\n",
               field_name, (unsigned)(capacity - 1));
        return ESP_ERR_INVALID_SIZE;
    }
    strlcpy(destination, value, capacity);
    return ESP_OK;
}

static esp_err_t update_config_field(sys_config_t *config,
                                     config_field_t field,
                                     const char *value)
{
    switch (field) {
    case CONFIG_FIELD_WIFI_SSID:
        return set_string(config->wifi_ssid, sizeof(config->wifi_ssid),
                          value, "wifi.ssid");
    case CONFIG_FIELD_WIFI_PASSWORD:
        return set_string(config->wifi_password, sizeof(config->wifi_password),
                          value, "wifi.password");
    case CONFIG_FIELD_DEVICE_NAME:
        if (value[0] == '\0') {
            printf("device.name 不能为空\n");
            return ESP_ERR_INVALID_ARG;
        }
        return set_string(config->device_name, sizeof(config->device_name),
                          value, "device.name");
    case CONFIG_FIELD_DEVICE_ID:
        return parse_device_id(value, &config->device_id);
    case CONFIG_FIELD_MDNS_HOSTNAME:
        if (!sys_config_mdns_hostname_is_valid(value)) {
            printf("mdns.hostname 只允许小写字母、数字和中划线，"
                   "且不能以中划线开头或结尾\n");
            return ESP_ERR_INVALID_ARG;
        }
        return set_string(config->mdns_hostname,
                          sizeof(config->mdns_hostname), value,
                          "mdns.hostname");
    default:
        return ESP_ERR_NOT_FOUND;
    }
}

static esp_err_t clear_config_field(sys_config_t *config,
                                    config_field_t field)
{
    switch (field) {
    case CONFIG_FIELD_WIFI_SSID:
        config->wifi_ssid[0] = '\0';
        return ESP_OK;
    case CONFIG_FIELD_WIFI_PASSWORD:
        config->wifi_password[0] = '\0';
        return ESP_OK;
    case CONFIG_FIELD_MDNS_HOSTNAME:
        config->mdns_hostname[0] = '\0';
        return ESP_OK;
    default:
        printf("该配置项不允许清空\n");
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static int do_cmd_config(int argc, char **argv)
{
    int errors = arg_parse(argc, argv, (void **)&config_args);
    if (errors != 0) {
        arg_print_errors(stderr, config_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    const char *action = config_args.action->sval[0];
    const char *key = config_args.key->count > 0
                          ? config_args.key->sval[0] : NULL;
    const char *value = config_args.value->count > 0
                            ? config_args.value->sval[0] : NULL;

    if (strcasecmp(action, "reset") == 0) {
        if (key || value) {
            printf("config reset 不接受 key 或 value 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = sys_config_reset_factory();
        if (err == ESP_OK) {
            printf("全部配置已恢复为出厂值\n");
        }
        return err;
    }

    sys_config_t config;
    esp_err_t err = sys_config_get(&config);
    if (err != ESP_OK) {
        printf("读取配置失败: %s\n", esp_err_to_name(err));
        return err;
    }

    if (strcasecmp(action, "show") == 0) {
        if (key || value) {
            printf("config show 不接受 key 或 value 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        print_all_config(&config);
        return ESP_OK;
    }

    if (!key) {
        printf("操作 %s 需要 key 参数，请使用 'help config' 查看用法\n",
               action);
        return ESP_ERR_INVALID_ARG;
    }

    config_field_t field = config_field_from_name(key);
    if (field == CONFIG_FIELD_UNKNOWN) {
        printf("未知配置键: %s\n", key);
        return ESP_ERR_NOT_FOUND;
    }

    if (strcasecmp(action, "get") == 0) {
        if (value) {
            printf("config get 不接受 value 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        print_config_field(&config, field);
        return ESP_OK;
    }

    if (strcasecmp(action, "set") == 0) {
        if (!value) {
            printf("config set 需要 value 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        err = update_config_field(&config, field, value);
    } else if (strcasecmp(action, "clear") == 0) {
        if (value) {
            printf("config clear 不接受 value 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        err = clear_config_field(&config, field);
    } else {
        printf("未知操作: %s，可用操作: show | get | set | clear | reset\n",
               action);
        return ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        return err;
    }
    err = sys_config_save(&config);
    if (err != ESP_OK) {
        printf("保存配置失败: %s\n", esp_err_to_name(err));
        return err;
    }

    printf("配置已保存: ");
    print_config_field(&config, field);
    printf("网络相关修改将在数秒内由网络任务自动应用\n");
    return ESP_OK;
}

esp_err_t register_cmd_config(void)
{
    config_args.action = arg_str1(
        NULL, NULL, "<show|get|set|clear|reset>",
        "操作: show(全部查看), get(单项查看), set(修改), clear(清空), reset(恢复默认)");
    config_args.key = arg_str0(
        NULL, NULL, "<key>",
        "配置键: wifi.ssid, wifi.password, device.name, device.id, mdns.hostname");
    config_args.value = arg_str0(
        NULL, NULL, "<value>",
        "set 操作的新值；包含空格时请使用双引号");
    config_args.end = arg_end(3);

    const esp_console_cmd_t command = {
        .command = "config",
        .help = "查看和修改系统配置（clear 仅支持 wifi.ssid、wifi.password、mdns.hostname）",
        .hint = NULL,
        .func = &do_cmd_config,
        .argtable = &config_args,
    };
    return esp_console_cmd_register(&command);
}

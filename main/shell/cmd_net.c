/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_net.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "argtable3/argtable3.h"
#include "config.h"
#include "esp_console.h"
#include "esp_netif.h"
#include "net_task.h"

static struct {
    struct arg_str *action;
    struct arg_str *subaction;
    struct arg_str *value;
    struct arg_end *end;
} net_args;

static const char *net_state_name(net_task_state_t state)
{
    switch (state) {
    case NET_TASK_STATE_STOPPED: return "stopped";
    case NET_TASK_STATE_WAITING_CONFIG: return "waiting-config";
    case NET_TASK_STATE_CONNECTING: return "connecting";
    case NET_TASK_STATE_WAITING_IP: return "waiting-ip";
    case NET_TASK_STATE_BACKOFF: return "backoff";
    case NET_TASK_STATE_ONLINE: return "online";
    default: return "unknown";
    }
}

static esp_err_t print_net_status(void)
{
    net_status_t status;
    esp_err_t err = net_get_status(&status);
    if (err != ESP_OK) {
        printf("读取网络状态失败: %s\n", esp_err_to_name(err));
        return err;
    }

    printf("state: %s\n", net_state_name(status.state));
    printf("ssid: %s\n", status.ssid[0] ? status.ssid : "(未配置)");
    printf("wifi.connected: %s\n",
           status.state_bits & NET_STATE_WIFI_CONNECTED ? "yes" : "no");
    printf("ipv4.ready: %s\n",
           status.state_bits & NET_STATE_IPV4_READY ? "yes" : "no");
    printf("ntp.synced: %s\n",
           status.state_bits & NET_STATE_TIME_SYNCED ? "yes" : "no");
    printf("mdns.ready: %s\n",
           status.state_bits & NET_STATE_MDNS_READY ? "yes" : "no");
    if (status.mdns_hostname[0]) {
        printf("mdns.address: %s.local\n", status.mdns_hostname);
    }
    if (status.state_bits & NET_STATE_IPV4_READY) {
        printf("ipv4.address: " IPSTR "\n", IP2STR(&status.ip_info.ip));
        printf("ipv4.gateway: " IPSTR "\n", IP2STR(&status.ip_info.gw));
        printf("ipv4.netmask: " IPSTR "\n", IP2STR(&status.ip_info.netmask));
    }
    printf("disconnect.reason: %u\n", status.disconnect_reason);
    printf("disconnect.rssi: %d dBm\n", status.disconnect_rssi);
    printf("reconnect.count: %lu\n", (unsigned long)status.reconnect_count);
    printf("reconnect.delay_ms: %lu\n",
           (unsigned long)status.reconnect_delay_ms);

    if (status.state_bits & NET_STATE_TIME_SYNCED) {
        char last_sync[32];
        struct tm sync_time;
        if (localtime_r(&status.last_sync_time, &sync_time) &&
            strftime(last_sync, sizeof(last_sync), "%Y-%m-%d %H:%M:%S %Z",
                     &sync_time) > 0) {
            printf("ntp.last_sync: %s\n", last_sync);
        }
    }
    return ESP_OK;
}

static esp_err_t print_net_time(void)
{
    time_t epoch;
    esp_err_t err = net_time_get_epoch(&epoch);
    if (err != ESP_OK) {
        printf("网络时间尚未同步\n");
        return err;
    }

    char formatted[40];
    err = net_time_format(formatted, sizeof(formatted),
                          "%Y-%m-%d %H:%M:%S %Z");
    if (err != ESP_OK) {
        return err;
    }
    printf("local: %s\n", formatted);
    printf("epoch: %lld\n", (long long)epoch);
    return ESP_OK;
}

static esp_err_t print_net_mdns(void)
{
    sys_config_t config;
    net_status_t status;
    esp_err_t err = sys_config_get(&config);
    if (err == ESP_OK) {
        err = net_get_status(&status);
    }
    if (err != ESP_OK) {
        printf("读取 mDNS 状态失败: %s\n", esp_err_to_name(err));
        return err;
    }

    if (config.mdns_hostname[0]) {
        printf("configured.hostname: %s\n", config.mdns_hostname);
    } else {
        printf("configured.hostname: (自动生成: ac-remote-%u)\n",
               config.device_id);
    }
    printf("ready: %s\n",
           status.state_bits & NET_STATE_MDNS_READY ? "yes" : "no");
    if (status.mdns_hostname[0]) {
        printf("active.hostname: %s\n", status.mdns_hostname);
        printf("address: http://%s.local/\n", status.mdns_hostname);
    }
    return ESP_OK;
}

static esp_err_t configure_net_mdns(const char *subaction,
                                    const char *value)
{
    if (!subaction) {
        return print_net_mdns();
    }

    sys_config_t config;
    esp_err_t err = sys_config_get(&config);
    if (err != ESP_OK) {
        return err;
    }

    if (strcasecmp(subaction, "reset") == 0) {
        if (value) {
            printf("net mdns reset 不接受 hostname 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        config.mdns_hostname[0] = '\0';
    } else if (strcasecmp(subaction, "set") == 0) {
        if (!value) {
            printf("net mdns set 需要 hostname 参数\n");
            return ESP_ERR_INVALID_ARG;
        }
        if (!sys_config_mdns_hostname_is_valid(value)) {
            printf("hostname 只允许小写字母、数字和中划线，且不能以中划线开头或结尾\n");
            return ESP_ERR_INVALID_ARG;
        }
        strlcpy(config.mdns_hostname, value, sizeof(config.mdns_hostname));
    } else {
        printf("未知 mDNS 操作: %s，可用操作: set | reset\n", subaction);
        return ESP_ERR_INVALID_ARG;
    }

    err = sys_config_save(&config);
    if (err != ESP_OK) {
        printf("保存 mDNS 配置失败: %s\n", esp_err_to_name(err));
        return err;
    }
    printf("mDNS 配置已保存，网络任务将在数秒内应用\n");
    return ESP_OK;
}

static int do_cmd_net(int argc, char **argv)
{
    int errors = arg_parse(argc, argv, (void **)&net_args);
    if (errors != 0) {
        arg_print_errors(stderr, net_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    const char *action = net_args.action->sval[0];
    const char *subaction = net_args.subaction->count > 0
                                ? net_args.subaction->sval[0] : NULL;
    const char *value = net_args.value->count > 0
                            ? net_args.value->sval[0] : NULL;

    if (strcasecmp(action, "mdns") == 0) {
        return configure_net_mdns(subaction, value);
    }
    if (subaction || value) {
        printf("net %s 不接受额外参数\n", action);
        return ESP_ERR_INVALID_ARG;
    }
    if (strcasecmp(action, "status") == 0) {
        return print_net_status();
    }
    if (strcasecmp(action, "time") == 0) {
        return print_net_time();
    }
    if (strcasecmp(action, "sync") == 0) {
        esp_err_t err = net_request_time_sync();
        if (err == ESP_OK) {
            printf("已请求 NTP 重新同步\n");
        }
        return err;
    }
    if (strcasecmp(action, "reconnect") == 0) {
        esp_err_t err = net_request_reconnect();
        if (err == ESP_OK) {
            printf("已请求 Wi-Fi 重新连接\n");
        }
        return err;
    }

    printf("未知操作: %s，可用操作: status | time | mdns | sync | reconnect\n",
           action);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t register_cmd_net(void)
{
    net_args.action = arg_str1(
        NULL, NULL, "<status|time|mdns|sync|reconnect>",
        "操作: 网络状态、当前时间、mDNS 配置、立即校时或重新连接");
    net_args.subaction = arg_str0(
        NULL, NULL, "<set|reset>", "mdns 的可选操作");
    net_args.value = arg_str0(
        NULL, NULL, "<hostname>", "mdns set 使用的主机名");
    net_args.end = arg_end(3);

    const esp_console_cmd_t command = {
        .command = "net",
        .help = "网络状态、时间同步、Wi-Fi 重连和 mDNS 调试",
        .hint = NULL,
        .func = &do_cmd_net,
        .argtable = &net_args,
    };
    return esp_console_cmd_register(&command);
}

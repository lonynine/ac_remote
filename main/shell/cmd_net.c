/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_net.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "esp_console.h"
#include "esp_netif.h"
#include "net_task.h"

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

static void print_net_help(void)
{
    printf("\n网络调试命令:\n");
    printf("  net status       查看 WiFi、IP、NTP 和重连状态\n");
    printf("  net time         查看当前网络校准时间\n");
    printf("  net sync         请求立即进行一次 NTP 校时\n");
    printf("  net reconnect    请求 WiFi 立即重新连接\n");
    printf("  net help         显示本帮助\n\n");
}

static int print_net_status(void)
{
    net_status_t status;
    esp_err_t err = net_get_status(&status);
    if (err != ESP_OK) {
        printf("读取网络状态失败: %s\n", esp_err_to_name(err));
        return err;
    }

    printf("网络任务状态: %s\n", net_state_name(status.state));
    printf("SSID: %s\n", status.ssid[0] ? status.ssid : "(未配置)");
    printf("WiFi连接: %s\n",
           status.state_bits & NET_STATE_WIFI_CONNECTED ? "是" : "否");
    printf("IPv4可用: %s\n",
           status.state_bits & NET_STATE_IPV4_READY ? "是" : "否");
    printf("NTP已同步: %s\n",
           status.state_bits & NET_STATE_TIME_SYNCED ? "是" : "否");

    if (status.state_bits & NET_STATE_IPV4_READY) {
        printf("IP: " IPSTR "\n", IP2STR(&status.ip_info.ip));
        printf("网关: " IPSTR "\n", IP2STR(&status.ip_info.gw));
        printf("掩码: " IPSTR "\n", IP2STR(&status.ip_info.netmask));
    }
    printf("最近断开: reason=%u rssi=%d dBm\n",
           status.disconnect_reason, status.disconnect_rssi);
    printf("重连次数: %lu", (unsigned long)status.reconnect_count);
    if (status.reconnect_delay_ms > 0) {
        printf("，下次等待 %lu ms", (unsigned long)status.reconnect_delay_ms);
    }
    printf("\n");

    if (status.state_bits & NET_STATE_TIME_SYNCED) {
        char last_sync[32];
        struct tm sync_tm;
        if (localtime_r(&status.last_sync_time, &sync_tm) &&
            strftime(last_sync, sizeof(last_sync), "%Y-%m-%d %H:%M:%S %Z",
                     &sync_tm) > 0) {
            printf("最后校时: %s\n", last_sync);
        }
    }
    return ESP_OK;
}

static int print_net_time(void)
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
        printf("格式化网络时间失败: %s\n", esp_err_to_name(err));
        return err;
    }
    printf("本地时间: %s\n", formatted);
    printf("Unix时间: %lld\n", (long long)epoch);
    return ESP_OK;
}

static int do_cmd_net(int argc, char **argv)
{
    if (argc < 2 || strcasecmp(argv[1], "help") == 0) {
        print_net_help();
        return ESP_OK;
    }
    if (strcasecmp(argv[1], "status") == 0) {
        return print_net_status();
    }
    if (strcasecmp(argv[1], "time") == 0) {
        return print_net_time();
    }
    if (strcasecmp(argv[1], "sync") == 0) {
        esp_err_t err = net_request_time_sync();
        if (err == ESP_OK) {
            printf("已请求 NTP 重新校时，请使用 net status 查看结果\n");
        } else {
            printf("请求 NTP 校时失败，请确认 IPv4 已可用: %s\n",
                   esp_err_to_name(err));
        }
        return err;
    }
    if (strcasecmp(argv[1], "reconnect") == 0) {
        esp_err_t err = net_request_reconnect();
        if (err == ESP_OK) {
            printf("已请求 WiFi 重新连接\n");
        } else {
            printf("请求 WiFi 重连失败: %s\n", esp_err_to_name(err));
        }
        return err;
    }

    printf("未知 net 子命令: %s\n", argv[1]);
    print_net_help();
    return ESP_ERR_INVALID_ARG;
}

esp_err_t register_cmd_net(void)
{
    const esp_console_cmd_t command = {
        .command = "net",
        .help = "网络状态与时间调试命令\n"
                "  net status\n"
                "  net time\n"
                "  net sync\n"
                "  net reconnect\n"
                "  net help",
        .hint = NULL,
        .func = do_cmd_net,
        .argtable = NULL,
    };
    return esp_console_cmd_register(&command);
}

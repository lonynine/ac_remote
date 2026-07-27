/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "config.h"
#include "wifi_sta.h"
#include "ble_server.h"
#include "shell.h"
#include "task_manager.h"

static const char *TAG = "main";

void app_main(void) {
  // 0. 日志等级控制 (全部使用极简 TAG: main, nvs, config, wifi, ble, shell, net_task, task_mgr)
  esp_log_level_set("*",        ESP_LOG_WARN);
  esp_log_level_set("main",     ESP_LOG_WARN);
  esp_log_level_set("nvs",      ESP_LOG_WARN);
  esp_log_level_set("config",   ESP_LOG_WARN);
  esp_log_level_set("wifi",     ESP_LOG_WARN);
  esp_log_level_set("ble",      ESP_LOG_WARN);
  esp_log_level_set("shell",    ESP_LOG_WARN);
  esp_log_level_set("net_task", ESP_LOG_WARN);
  esp_log_level_set("task_mgr", ESP_LOG_WARN);

  // 1. 初始化系统配置与任务管理中间层
  ESP_ERROR_CHECK(sys_config_init());
  ESP_ERROR_CHECK(task_manager_init());

  // 2. 读取并一次性格式化打印全部系统配置参数 (密码明文打印)
  sys_config_t cfg;
  ESP_ERROR_CHECK(sys_config_get(&cfg));

  printf("\n================ [ 系统配置参数 ] ================\n");
  printf("  设备名称 (Device Name) : %s\n", cfg.device_name);
  printf("  设备 ID  (Device ID)   : %d\n", cfg.device_id);
  printf("  WiFi SSID              : %s\n", strlen(cfg.wifi_ssid) > 0 ? cfg.wifi_ssid : "(未配置)");
  printf("  WiFi 密码 (Password)   : %s\n", strlen(cfg.wifi_password) > 0 ? cfg.wifi_password : "(未配置)");
  printf("==================================================\n\n");

  // 3. 通过中间层统一启动网络任务
  task_manager_start("net");

  // 4. 启动 BLE 蓝牙服务
  ble_server_init(cfg.device_name);

  // 5. 启动 Shell 控制台
  ESP_ERROR_CHECK(shell_init());
  shell_start();
}

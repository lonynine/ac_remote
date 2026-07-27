/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ac_state.h"
#include "aht20.h"
#include "ble_server.h"
#include "config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "i2c_driver.h"
#include "ir_remote.h"
#include "nvs_driver.h"
#include "sensor_task.h"
#include "shell.h"
#include "task_manager.h"
#include "wifi_sta.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "main";

void app_main(void) {
  ESP_LOGI(TAG, "ESP32-S3 空调主控系统启动中...");
  // 0. 日志等级控制
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set("main", ESP_LOG_WARN);
  esp_log_level_set("nvs", ESP_LOG_WARN);
  esp_log_level_set("config", ESP_LOG_WARN);
  esp_log_level_set("ac_state", ESP_LOG_WARN);
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("ble", ESP_LOG_WARN);
  esp_log_level_set("shell", ESP_LOG_WARN);
  esp_log_level_set("net_task", ESP_LOG_WARN);
  esp_log_level_set("task_mgr", ESP_LOG_WARN);
  esp_log_level_set("i2c_driver", ESP_LOG_WARN);
  esp_log_level_set("aht20", ESP_LOG_WARN);

  // 放开控制任务、传感器采集任务与红外发送的 INFO 日志
  esp_log_level_set("control_task", ESP_LOG_INFO);
  esp_log_level_set("sensor_task", ESP_LOG_WARN);
  esp_log_level_set("ir_remote", ESP_LOG_INFO);

  // 1. 初始化底层硬件驱动 (NVS, I2C, AHT20, 红外收发)
  ESP_ERROR_CHECK(sys_config_init());
  ESP_ERROR_CHECK(i2c_driver_init());
  ESP_ERROR_CHECK(aht20_init());
  ESP_ERROR_CHECK(ir_remote_init());
  ESP_ERROR_CHECK(ac_state_init());
  ESP_ERROR_CHECK(task_manager_init());

  // 2. 读取并打印系统配置参数
  sys_config_t cfg;
  ESP_ERROR_CHECK(sys_config_get(&cfg));

  printf("\n================ [ 系统配置参数 ] ================\n");
  printf("  设备名称 (Device Name) : %s\n", cfg.device_name);
  printf("  设备 ID  (Device ID)   : %d\n", cfg.device_id);
  printf("  WiFi SSID              : %s\n",
         strlen(cfg.wifi_ssid) > 0 ? cfg.wifi_ssid : "(未配置)");
  printf("  WiFi 密码 (Password)   : %s\n",
         strlen(cfg.wifi_password) > 0 ? cfg.wifi_password : "(未配置)");
  printf("  I2C 总线 (SCL / SDA)   : IO47 / IO48 (AHT20 温湿度)\n");
  printf("  红外发送 / 接收 (TX/RX): IO4 / IO5\n");
  printf("==================================================\n\n");

  // 3. 通过任务管理中间层统一启动后台任务 (网络、红外控制、温湿度传感器采集)
  task_manager_start("net");
  task_manager_start("control");
  task_manager_start("sensor");

  // 4. 启动 BLE 蓝牙服务
  ble_server_init(cfg.device_name);

  // 5. 启动 Shell 控制台
  ESP_ERROR_CHECK(shell_init());
  shell_start();
}

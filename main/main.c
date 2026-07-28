/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ac_state.h"
#include "ble_server.h"
#include "config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "i2c_driver.h"
#include "ir_remote.h"
#include "shell.h"
#include "task_manager.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "main";

static void setup_log_levels(void)
{
  esp_log_level_set("*", ESP_LOG_WARN);

  // esp_log_level_set("main", ESP_LOG_INFO);
  // esp_log_level_set("config", ESP_LOG_INFO);
  // esp_log_level_set("nvs", ESP_LOG_INFO);
  // esp_log_level_set("ac", ESP_LOG_INFO);
  // esp_log_level_set("task", ESP_LOG_INFO);
  // esp_log_level_set("net", ESP_LOG_INFO);
  // esp_log_level_set("wifi", ESP_LOG_INFO);
  // esp_log_level_set("ble", ESP_LOG_INFO);
  // esp_log_level_set("sensor", ESP_LOG_INFO);
  // esp_log_level_set("i2c", ESP_LOG_INFO);
  // esp_log_level_set("rmt", ESP_LOG_INFO);
  // esp_log_level_set("ir", ESP_LOG_INFO);
  // esp_log_level_set("proto", ESP_LOG_INFO);
  // esp_log_level_set("ctrl", ESP_LOG_INFO);
  // esp_log_level_set("shell", ESP_LOG_INFO);
}

static bool log_step_result(const char *name, esp_err_t err)
{
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s failed: %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

void app_main(void) {
  setup_log_levels();

  // 1. 初始化底层硬件驱动 (NVS, I2C, AHT20, 红外收发)
  if (!log_step_result("config init", sys_config_init())) return;
  if (!log_step_result("i2c init", i2c_driver_init())) return;
  if (!log_step_result("ir init", ir_remote_init())) return;
  if (!log_step_result("ac state init", ac_state_init())) return;
  if (!log_step_result("task manager init", task_manager_init())) return;

  // 2. 读取并打印系统配置参数
  sys_config_t cfg;
  if (!log_step_result("config read", sys_config_get(&cfg))) return;

  ESP_LOGI(TAG, "device=%s id=%d wifi_ssid=%s wifi_pass=%s i2c=47/48 ir=4/5",
           cfg.device_name, cfg.device_id,
           strlen(cfg.wifi_ssid) > 0 ? cfg.wifi_ssid : "(empty)",
           strlen(cfg.wifi_password) > 0 ? cfg.wifi_password : "(empty)");

  // 3. 通过任务管理中间层统一启动后台任务 (网络、红外控制、温湿度传感器采集)
  log_step_result("net task start", task_manager_start("net"));
  log_step_result("control task start", task_manager_start("control"));
  log_step_result("sensor task start", task_manager_start("sensor"));

  // 4. 启动 BLE 蓝牙服务
  log_step_result("ble init", ble_server_init(cfg.device_name));

  // 5. 启动 Shell 控制台
  if (!log_step_result("shell init", shell_init())) return;
  shell_start();
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "task_manager.h"
#include "net_task.h"
#include "control_task.h"
#include "sensor_task.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "task";

// 业务任务注册表。底层能力通过 network 模块提供。
static const task_item_t s_task_registry[] = {
    {
        .name = "net",
        .description = "WiFi 重连、网络状态与 NTP 同步管理任务",
        .start = net_task_start,
        .stop = net_task_stop,
        .is_running = net_task_is_running,
    },
    {
        .name = "control",
        .description = "空调红外遥控数据发送与消息队列监听任务",
        .start = control_task_start,
        .stop = control_task_stop,
        .is_running = control_task_is_running,
    },
    {
        .name = "sensor",
        .description = "AHT20 温湿度传感器定时采集任务",
        .start = sensor_task_start,
        .stop = sensor_task_stop,
        .is_running = sensor_task_is_running,
    },
};

#define TASK_REGISTRY_SIZE (sizeof(s_task_registry) / sizeof(s_task_registry[0]))

esp_err_t task_manager_init(void)
{
    ESP_LOGI(TAG, "任务管理中间层初始化完成 (已注册 %d 个任务)", (int)TASK_REGISTRY_SIZE);
    return ESP_OK;
}

static const task_item_t* find_task(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < TASK_REGISTRY_SIZE; i++) {
        if (strcmp(s_task_registry[i].name, name) == 0) {
            return &s_task_registry[i];
        }
    }
    return NULL;
}

esp_err_t task_manager_start(const char *name)
{
    const task_item_t *item = find_task(name);
    if (!item) {
        return ESP_ERR_NOT_FOUND;
    }
    if (item->start) {
        return item->start();
    }
    return ESP_FAIL;
}

esp_err_t task_manager_stop(const char *name)
{
    const task_item_t *item = find_task(name);
    if (!item) {
        return ESP_ERR_NOT_FOUND;
    }
    if (item->stop) {
        return item->stop();
    }
    return ESP_FAIL;
}

bool task_manager_is_running(const char *name)
{
    const task_item_t *item = find_task(name);
    if (!item) {
        return false;
    }
    if (item->is_running) {
        return item->is_running();
    }
    return false;
}

void task_manager_print_status(void)
{
    printf("\n================ [ 系统后台任务运行状态 ] ================\n");
    for (size_t i = 0; i < TASK_REGISTRY_SIZE; i++) {
        bool running = s_task_registry[i].is_running ? s_task_registry[i].is_running() : false;
        printf("  任务名称: %-10s | 状态: %s | 描述: %s\n",
               s_task_registry[i].name,
               running ? "🟢 [运行中]" : "🔴 [已停止]",
               s_task_registry[i].description);
    }
    printf("==========================================================\n\n");
}

void task_manager_print_all_status(void)
{
    task_manager_print_status();
}

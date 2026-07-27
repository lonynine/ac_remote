/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "task_manager.h"
#include "net_task.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "task_mgr";

// 表驱动注册表：未来增加任何新 Task，只需在此处追加一行注册即可！
static const task_item_t s_task_registry[] = {
    {
        .name = "net",
        .description = "WiFi 网络管理与自动重连监控任务",
        .start = net_task_start,
        .stop = net_task_stop,
        .is_running = net_task_is_running,
    },
};

#define TASK_REGISTRY_SIZE (sizeof(s_task_registry) / sizeof(s_task_registry[0]))

esp_err_t task_manager_init(void)
{
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
    return ESP_ERR_NOT_SUPPORTED;
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
    return ESP_ERR_NOT_SUPPORTED;
}

bool task_manager_is_running(const char *name)
{
    const task_item_t *item = find_task(name);
    if (item && item->is_running) {
        return item->is_running();
    }
    return false;
}

void task_manager_print_all_status(void)
{
    printf("\n================ [ 系统任务管理中间层状态 ] ================\n");
    for (size_t i = 0; i < TASK_REGISTRY_SIZE; i++) {
        bool running = s_task_registry[i].is_running ? s_task_registry[i].is_running() : false;
        printf("  [%zu] 任务标识 (Name) : %-10s | 描述: %s\n", i + 1, s_task_registry[i].name, s_task_registry[i].description);
        printf("      运行状态 (Status): %s\n", running ? "正在运行 (Running)" : "已停止 (Stopped)");
        if (i < TASK_REGISTRY_SIZE - 1) {
            printf("  ---------------------------------------------------------\n");
        }
    }
    printf("============================================================\n\n");
}

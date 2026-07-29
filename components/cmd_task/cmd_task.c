/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_task.h"
#include "task_manager.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"

static struct {
    struct arg_str *action;
    struct arg_str *name;
    struct arg_end *end;
} task_cmd_args;

static int do_task_cmd(int argc, char **argv)
{
    int errors = arg_parse(argc, argv, (void **)&task_cmd_args);
    if (errors != 0) {
        arg_print_errors(stderr, task_cmd_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    const char *action = task_cmd_args.action->sval[0];
    const char *name = (task_cmd_args.name->count > 0) ? task_cmd_args.name->sval[0] : "net";

    if (!task_manager_exists(name)) {
        printf("未知任务: %s\n", name);
        return ESP_ERR_NOT_FOUND;
    }

    if (strcasecmp(action, "start") == 0) {
        if (task_manager_is_running(name)) {
            printf("task %s: running\n", name);
            return ESP_OK;
        } else {
            esp_err_t err = task_manager_start(name);
            if (err == ESP_OK) {
                printf("task %s: started\n", name);
            } else {
                printf("启动任务 %s 失败: %s\n", name, esp_err_to_name(err));
            }
            return err;
        }
    } else if (strcasecmp(action, "stop") == 0) {
        if (!task_manager_is_running(name)) {
            printf("task %s: stopped\n", name);
            return ESP_OK;
        } else {
            esp_err_t err = task_manager_stop(name);
            if (err == ESP_OK) {
                printf("task %s: stopped\n", name);
            } else {
                printf("停止任务 %s 失败: %s\n", name, esp_err_to_name(err));
            }
            return err;
        }
    } else if (strcasecmp(action, "status") == 0) {
        if (task_cmd_args.name->count > 0) {
            printf("task %s: %s\n", name,
                   task_manager_is_running(name) ? "running" : "stopped");
            return ESP_OK;
        }
        task_manager_print_all_status();
        return ESP_OK;
    } else {
        printf("未知操作: %s，可用操作: start | stop | status\n", action);
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t register_cmd_task(void)
{
    task_cmd_args.action = arg_str1(
        NULL, NULL, "<start|stop|status>",
        "操作: 启动、停止或查看任务状态");
    task_cmd_args.name = arg_str0(
        NULL, NULL, "<task_name>",
        "任务名称: net、http、control、sensor；start/stop 默认 net");
    task_cmd_args.end    = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "task",
        .help = "管理应用后台任务",
        .hint = NULL,
        .func = &do_task_cmd,
        .argtable = &task_cmd_args
    };

    return esp_console_cmd_register(&cmd);
}

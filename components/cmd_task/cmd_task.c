/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cmd_task.h"
#include "task_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"

static struct {
    struct arg_str *action;
    struct arg_str *name;
    struct arg_end *end;
} task_cmd_args;

static int do_task_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&task_cmd_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, task_cmd_args.end, argv[0]);
        return 1;
    }

    const char *action = task_cmd_args.action->sval[0];
    const char *name = (task_cmd_args.name->count > 0) ? task_cmd_args.name->sval[0] : "net";

    if (strcmp(action, "start") == 0) {
        if (task_manager_is_running(name)) {
            printf("任务 [%s] 已经在运行中 (Running)\n", name);
        } else {
            esp_err_t err = task_manager_start(name);
            if (err == ESP_OK) {
                printf("成功启动任务 [%s]\n", name);
            } else if (err == ESP_ERR_NOT_FOUND) {
                printf("未找到任务 [%s]\n", name);
            } else {
                printf("启动任务 [%s] 失败: %s\n", name, esp_err_to_name(err));
            }
        }
    } else if (strcmp(action, "stop") == 0) {
        if (!task_manager_is_running(name)) {
            printf("任务 [%s] 当前未运行 (Stopped)\n", name);
        } else {
            esp_err_t err = task_manager_stop(name);
            if (err == ESP_OK) {
                printf("已成功停止任务 [%s]\n", name);
            } else {
                printf("停止任务 [%s] 失败: %s\n", name, esp_err_to_name(err));
            }
        }
    } else if (strcmp(action, "status") == 0) {
        task_manager_print_all_status();
    } else {
        printf("未知操作指令: %s。可用指令: start | stop | status\n", action);
    }

    return 0;
}

esp_err_t register_cmd_task(void)
{
    task_cmd_args.action = arg_str1(NULL, NULL, "<start|stop|status>", "操作指令: start (启动), stop (停止), status (状态)");
    task_cmd_args.name   = arg_str0(NULL, NULL, "[task_name]", "可选参数: 任务名称 (默认为 net)");
    task_cmd_args.end    = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "task",
        .help = "任务管理控制指令 (用法: task <start|stop|status> [task_name])",
        .hint = NULL,
        .func = &do_task_cmd,
        .argtable = &task_cmd_args
    };

    return esp_console_cmd_register(&cmd);
}

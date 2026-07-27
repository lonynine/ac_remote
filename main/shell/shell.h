/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SHELL_H
#define SHELL_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Shell 模块 (包含外设、文件系统、Linenoise 及命令注册)
 *
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t shell_init(void);

/**
 * @brief 启动 Shell 命令行交互循环 (阻塞运行)
 */
void shell_start(void);

#ifdef __cplusplus
}
#endif

#endif // SHELL_H

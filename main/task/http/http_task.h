/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HTTP_TASK_H
#define HTTP_TASK_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t http_task_start(void);
esp_err_t http_task_stop(void);
bool http_task_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_TASK_H

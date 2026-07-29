/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WEB_UI_H
#define WEB_UI_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_ui_handler(httpd_req_t *request);
esp_err_t web_favicon_handler(httpd_req_t *request);

#ifdef __cplusplus
}
#endif

#endif // WEB_UI_H

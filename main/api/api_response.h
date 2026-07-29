/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef API_RESPONSE_H
#define API_RESPONSE_H

#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t api_send_json(httpd_req_t *request, const char *status,
                        const char *code, const char *message, cJSON *data);

#ifdef __cplusplus
}
#endif

#endif // API_RESPONSE_H

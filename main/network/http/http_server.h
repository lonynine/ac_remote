/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORK_HTTP_SERVER_H
#define NETWORK_HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*network_http_handler_t)(httpd_req_t *request);

esp_err_t network_http_server_start(uint16_t port);
esp_err_t network_http_server_stop(void);
bool network_http_server_is_running(void);
esp_err_t network_http_register_route(const char *uri,
                                      httpd_method_t method,
                                      network_http_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_HTTP_SERVER_H

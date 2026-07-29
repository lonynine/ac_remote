/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "http_server.h"

#include "esp_log.h"

static const char *TAG = "http";
static httpd_handle_t s_server;

esp_err_t network_http_server_start(uint16_t port)
{
    if (port == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_server) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 16;
    config.stack_size = 6144;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        s_server = NULL;
    }
    return err;
}

esp_err_t network_http_server_stop(void)
{
    if (!s_server) {
        return ESP_OK;
    }

    httpd_handle_t server = s_server;
    s_server = NULL;
    return httpd_stop(server);
}

bool network_http_server_is_running(void)
{
    return s_server != NULL;
}

esp_err_t network_http_register_route(const char *uri,
                                      httpd_method_t method,
                                      network_http_handler_t handler)
{
    if (!s_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!uri || uri[0] == '\0' || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    const httpd_uri_t route = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL,
    };
    return httpd_register_uri_handler(s_server, &route);
}

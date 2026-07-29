/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "http_task.h"

#include "api_routes.h"
#include "esp_log.h"
#include "http_server.h"
#include "mdns_responder.h"

#define HTTP_SERVER_PORT 80

static const char *TAG = "http_task";

esp_err_t http_task_start(void)
{
    if (network_http_server_is_running()) {
        return ESP_OK;
    }

    esp_err_t err = network_http_server_start(HTTP_SERVER_PORT);
    if (err != ESP_OK) {
        return err;
    }
    err = api_routes_register();
    if (err != ESP_OK) {
        network_http_server_stop();
        return err;
    }
    err = network_mdns_add_service("_http", "_tcp", HTTP_SERVER_PORT);
    if (err != ESP_OK) {
        network_http_server_stop();
        return err;
    }
    ESP_LOGI(TAG, "HTTP API listening on port %u", HTTP_SERVER_PORT);
    return ESP_OK;
}

esp_err_t http_task_stop(void)
{
    network_mdns_remove_service("_http", "_tcp");
    return network_http_server_stop();
}

bool http_task_is_running(void)
{
    return network_http_server_is_running();
}

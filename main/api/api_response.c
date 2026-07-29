/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "api_response.h"

#include <stdbool.h>

esp_err_t api_send_json(httpd_req_t *request, const char *status,
                        const char *code, const char *message, cJSON *data)
{
    if (!request || !status || !code || !message) {
        cJSON_Delete(data);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *response = cJSON_CreateObject();
    if (!response) {
        cJSON_Delete(data);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "out of memory");
    }

    bool success = cJSON_AddStringToObject(response, "code", code) &&
                   cJSON_AddStringToObject(response, "message", message);
    if (success && data) {
        success = cJSON_AddItemToObject(response, "data", data);
        if (success) {
            data = NULL;
        }
    }
    cJSON_Delete(data);
    if (!success) {
        cJSON_Delete(response);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "out of memory");
    }

    char *body = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!body) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "out of memory");
    }

    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(request, body);
    cJSON_free(body);
    return err;
}

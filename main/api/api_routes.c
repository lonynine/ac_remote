/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "api_routes.h"

#include <stdlib.h>
#include <string.h>

#include "ac_state.h"
#include "api_response.h"
#include "config.h"
#include "control_task.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "http_server.h"
#include "net_task.h"
#include "protocol_manager.h"
#include "sensor_task.h"
#include "web_ui.h"

#define API_MAX_REQUEST_BODY 1024

static const char *mode_name(ac_mode_t mode)
{
    static const char *names[] = {"auto", "cool", "dry", "fan", "heat"};
    return mode <= AC_MODE_HEAT ? names[mode] : "unknown";
}

static const char *fan_name(ac_fan_t fan)
{
    static const char *names[] = {"auto", "low", "med", "high"};
    return fan <= AC_FAN_HIGH ? names[fan] : "unknown";
}

static const char *timer_name(ac_timer_mode_t timer)
{
    switch (timer) {
    case AC_TIMER_OFF: return "off";
    case AC_TIMER_ON: return "on";
    case AC_TIMER_ON_THEN_OFF: return "on_then_off";
    case AC_TIMER_OFF_THEN_ON: return "off_then_on";
    default: return "none";
    }
}

static esp_err_t parse_brand(const char *value, ac_brand_t *brand)
{
    if (strcmp(value, "haier") == 0) {
        *brand = AC_BRAND_HAIER;
    } else if (strcmp(value, "gree") == 0) {
        *brand = AC_BRAND_GREE;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t parse_mode(const char *value, ac_mode_t *mode)
{
    static const char *names[] = {"auto", "cool", "dry", "fan", "heat"};
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        if (strcmp(value, names[index]) == 0) {
            *mode = (ac_mode_t)index;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t parse_fan(const char *value, ac_fan_t *fan)
{
    static const char *names[] = {"auto", "low", "med", "high"};
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        if (strcmp(value, names[index]) == 0) {
            *fan = (ac_fan_t)index;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

static bool object_has_only(const cJSON *object, const char *const *names,
                            size_t name_count)
{
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, object) {
        bool known = false;
        for (size_t index = 0; index < name_count; index++) {
            if (strcmp(item->string, names[index]) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            return false;
        }
    }
    return true;
}

static esp_err_t receive_json(httpd_req_t *request, cJSON **root)
{
    if (request->content_len <= 0 || request->content_len > API_MAX_REQUEST_BODY) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *body = calloc(1, request->content_len + 1);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }

    size_t received = 0;
    while (received < request->content_len) {
        int length = httpd_req_recv(request, body + received,
                                    request->content_len - received);
        if (length == HTTPD_SOCK_ERR_TIMEOUT) {
            free(body);
            return ESP_ERR_TIMEOUT;
        }
        if (length <= 0) {
            free(body);
            return ESP_FAIL;
        }
        received += (size_t)length;
    }

    *root = cJSON_ParseWithLength(body, received);
    free(body);
    return *root ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t api_index_handler(httpd_req_t *request)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return api_send_json(request, "500 Internal Server Error", "no_memory",
                             "out of memory", NULL);
    }
    cJSON *endpoints = cJSON_AddArrayToObject(data, "endpoints");
    if (!endpoints) {
        cJSON_Delete(data);
        return api_send_json(request, "500 Internal Server Error", "no_memory",
                             "out of memory", NULL);
    }
    cJSON_AddItemToArray(endpoints, cJSON_CreateString("GET /api/v1/system/status"));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString("GET /api/v1/sensors"));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString("GET /api/v1/ac/state"));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString("POST /api/v1/ac/actions"));
    return api_send_json(request, "200 OK", "ok", "AC Remote API v1", data);
}

static esp_err_t system_status_handler(httpd_req_t *request)
{
    sys_config_t config;
    net_status_t network;
    if (sys_config_get(&config) != ESP_OK || net_get_status(&network) != ESP_OK) {
        return api_send_json(request, "503 Service Unavailable", "unavailable",
                             "system status is unavailable", NULL);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "device_name", config.device_name);
    cJSON_AddNumberToObject(data, "device_id", config.device_id);
    cJSON_AddNumberToObject(data, "uptime_seconds",
                           esp_timer_get_time() / 1000000LL);
    cJSON_AddNumberToObject(data, "free_heap", esp_get_free_heap_size());

    cJSON *net = cJSON_AddObjectToObject(data, "network");
    cJSON_AddBoolToObject(net, "wifi_connected",
                         network.state_bits & NET_STATE_WIFI_CONNECTED);
    cJSON_AddBoolToObject(net, "ipv4_ready",
                         network.state_bits & NET_STATE_IPV4_READY);
    cJSON_AddBoolToObject(net, "time_synced",
                         network.state_bits & NET_STATE_TIME_SYNCED);
    cJSON_AddBoolToObject(net, "mdns_ready",
                         network.state_bits & NET_STATE_MDNS_READY);
    cJSON_AddStringToObject(net, "hostname", network.mdns_hostname);

    float temperature = 0;
    float humidity = 0;
    cJSON *sensor = cJSON_AddObjectToObject(data, "sensor");
    esp_err_t sensor_err = sensor_task_get_data(&temperature, &humidity);
    cJSON_AddBoolToObject(sensor, "valid", sensor_err == ESP_OK);
    if (sensor_err == ESP_OK) {
        cJSON_AddNumberToObject(sensor, "temperature", temperature);
        cJSON_AddNumberToObject(sensor, "humidity", humidity);
    }
    return api_send_json(request, "200 OK", "ok", "system status", data);
}

static esp_err_t ac_state_handler(httpd_req_t *request)
{
    ac_state_t state;
    if (ac_state_get(&state) != ESP_OK) {
        return api_send_json(request, "503 Service Unavailable", "unavailable",
                             "AC state is unavailable", NULL);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "brand", ac_protocol_brand_name(state.brand));
    cJSON_AddBoolToObject(data, "power", state.power);
    cJSON_AddStringToObject(data, "mode", mode_name(state.mode));
    cJSON_AddNumberToObject(data, "temperature", state.temp);
    cJSON_AddStringToObject(data, "fan", fan_name(state.fan));
    cJSON_AddBoolToObject(data, "swing", state.swing);
    cJSON_AddBoolToObject(data, "light", state.light);
    cJSON *timer = cJSON_AddObjectToObject(data, "timer");
    cJSON_AddStringToObject(timer, "mode", timer_name(state.timer_mode));
    cJSON_AddNumberToObject(timer, "on_minutes", state.on_timer_min);
    cJSON_AddNumberToObject(timer, "off_minutes", state.off_timer_min);
    return api_send_json(request, "200 OK", "ok", "cached AC state", data);
}

static esp_err_t sensor_handler(httpd_req_t *request)
{
    sensor_data_t sensor;
    esp_err_t err = sensor_task_get_status(&sensor);

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return api_send_json(request, "500 Internal Server Error", "no_memory",
                             "out of memory", NULL);
    }
    cJSON_AddBoolToObject(data, "valid", err == ESP_OK);
    if (err == ESP_OK) {
        int64_t age_us = esp_timer_get_time() - sensor.updated_at_us;
        cJSON_AddNumberToObject(data, "temperature", sensor.temperature);
        cJSON_AddNumberToObject(data, "humidity", sensor.humidity);
        cJSON_AddNumberToObject(data, "age_ms",
                               age_us > 0 ? age_us / 1000 : 0);
    }
    return api_send_json(request, "200 OK", "ok", "latest sensor data", data);
}

static esp_err_t apply_optional_params(const cJSON *params, ac_request_t *command)
{
    static const char *const fields[] = {
        "brand", "power", "mode", "temperature", "fan",
        "swing", "light", "minutes",
    };
    if (!object_has_only(params, fields, sizeof(fields) / sizeof(fields[0]))) {
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "brand");
    if (value && (!cJSON_IsString(value) ||
                  parse_brand(value->valuestring, &command->brand) != ESP_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "power");
    if (value) {
        if (!cJSON_IsBool(value)) return ESP_ERR_INVALID_ARG;
        command->power = cJSON_IsTrue(value);
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "mode");
    if (value && (!cJSON_IsString(value) ||
                  parse_mode(value->valuestring, &command->mode) != ESP_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "temperature");
    if (value) {
        if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint ||
            value->valueint < 16 || value->valueint > 30) {
            return ESP_ERR_INVALID_ARG;
        }
        command->temp = (uint8_t)value->valueint;
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "fan");
    if (value && (!cJSON_IsString(value) ||
                  parse_fan(value->valuestring, &command->fan) != ESP_OK)) {
        return ESP_ERR_INVALID_ARG;
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "swing");
    if (value) {
        if (!cJSON_IsBool(value)) return ESP_ERR_INVALID_ARG;
        command->swing = cJSON_IsTrue(value);
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "light");
    if (value) {
        if (!cJSON_IsBool(value)) return ESP_ERR_INVALID_ARG;
        command->light = cJSON_IsTrue(value);
    }
    value = cJSON_GetObjectItemCaseSensitive(params, "minutes");
    if (value) {
        if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint ||
            value->valueint < 1 || value->valueint > 1439) {
            return ESP_ERR_INVALID_ARG;
        }
        command->timer_minutes = (uint16_t)value->valueint;
    }
    return ESP_OK;
}

static esp_err_t action_handler(httpd_req_t *request)
{
    cJSON *root = NULL;
    esp_err_t err = receive_json(request, &root);
    if (err == ESP_ERR_INVALID_SIZE) {
        return api_send_json(request, "413 Payload Too Large", "payload_too_large",
                             "request body must be 1-1024 bytes", NULL);
    }
    if (err != ESP_OK) {
        if (err == ESP_ERR_TIMEOUT) {
            return api_send_json(request, "408 Request Timeout", "request_timeout",
                                 "request body receive timeout", NULL);
        }
        return api_send_json(request, "400 Bad Request", "invalid_json",
                             "request body must be valid JSON", NULL);
    }

    static const char *const root_fields[] = {"target", "action", "params"};
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(root, "target");
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    if (!cJSON_IsObject(root) ||
        !object_has_only(root, root_fields,
                         sizeof(root_fields) / sizeof(root_fields[0])) ||
        !cJSON_IsString(target) || strcmp(target->valuestring, "ac") != 0 ||
        !cJSON_IsString(action) || !cJSON_IsObject(params)) {
        cJSON_Delete(root);
        return api_send_json(request, "400 Bad Request", "invalid_request",
                             "target, action and params are required", NULL);
    }

    ac_state_t state;
    if (ac_state_get(&state) != ESP_OK) {
        cJSON_Delete(root);
        return api_send_json(request, "503 Service Unavailable", "unavailable",
                             "AC state is unavailable", NULL);
    }
    ac_request_t command = {
        .brand = state.brand,
        .action = AC_ACTION_SET_STATE,
        .power = state.power,
        .mode = state.mode,
        .temp = state.temp,
        .fan = state.fan,
        .swing = state.swing,
        .light = state.light,
    };

    if (strcmp(action->valuestring, "set_state") == 0) {
        command.action = AC_ACTION_SET_STATE;
    } else if (strcmp(action->valuestring, "timer_on") == 0) {
        command.action = AC_ACTION_TIMER_ON;
    } else if (strcmp(action->valuestring, "timer_off") == 0) {
        command.action = AC_ACTION_TIMER_OFF;
    } else if (strcmp(action->valuestring, "timer_cancel") == 0) {
        command.action = AC_ACTION_TIMER_CANCEL;
    } else {
        cJSON_Delete(root);
        return api_send_json(request, "400 Bad Request", "unknown_action",
                             "unsupported action", NULL);
    }

    err = apply_optional_params(params, &command);
    cJSON_Delete(root);
    if (err != ESP_OK ||
        ((command.action == AC_ACTION_TIMER_ON ||
          command.action == AC_ACTION_TIMER_OFF) && command.timer_minutes == 0)) {
        return api_send_json(request, "400 Bad Request", "invalid_params",
                             "one or more params are invalid", NULL);
    }
    if (!ac_protocol_supports(command.brand, command.action)) {
        return api_send_json(request, "422 Unprocessable Entity", "not_supported",
                             "brand does not support this action", NULL);
    }

    err = control_task_post_request(&command);
    if (err != ESP_OK) {
        return api_send_json(request, "503 Service Unavailable", "queue_unavailable",
                             "control queue is unavailable", NULL);
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", "queued");
    return api_send_json(request, "202 Accepted", "accepted",
                         "action queued for execution", data);
}

esp_err_t api_routes_register(void)
{
    ESP_RETURN_ON_ERROR(network_http_register_route("/", HTTP_GET,
                                                     web_ui_handler),
                        "api", "register web UI route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/favicon.svg", HTTP_GET,
                                                     web_favicon_handler),
                        "api", "register SVG favicon route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/favicon.ico", HTTP_GET,
                                                     web_favicon_handler),
                        "api", "register favicon compatibility route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/api/v1", HTTP_GET,
                                                     api_index_handler),
                        "api", "register API index route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/api/v1/system/status",
                                                     HTTP_GET,
                                                     system_status_handler),
                        "api", "register system route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/api/v1/ac/state",
                                                     HTTP_GET,
                                                     ac_state_handler),
                        "api", "register AC state route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/api/v1/sensors",
                                                     HTTP_GET,
                                                     sensor_handler),
                        "api", "register sensor route");
    ESP_RETURN_ON_ERROR(network_http_register_route("/api/v1/ac/actions",
                                                     HTTP_POST,
                                                     action_handler),
                        "api", "register AC action route");
    return ESP_OK;
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "net_task.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mdns/mdns_responder.h"
#include "ntp/ntp_client.h"
#include "sta/wifi_sta.h"

#define NET_EVENT_QUEUE_LENGTH       12
#define NET_CONFIG_CHECK_MS          3000U
#define NET_RECONNECT_INITIAL_MS     1000U
#define NET_RECONNECT_MAX_MS         30000U
#define NET_NTP_SERVER               "pool.ntp.org"
#define NET_TIMEZONE                 "HKT-8"

typedef enum {
    NET_INTERNAL_WIFI_EVENT = 0,
    NET_INTERNAL_TIME_SYNCED,
    NET_INTERNAL_RECONNECT,
    NET_INTERNAL_SYNC_TIME,
} net_internal_event_id_t;

typedef struct {
    net_internal_event_id_t id;
    wifi_sta_event_t wifi;
    time_t sync_time;
} net_internal_event_t;

static const char *TAG = "net";
static TaskHandle_t s_net_task_handle;
static QueueHandle_t s_event_queue;
static EventGroupHandle_t s_state_events;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static net_status_t s_status;
static bool s_ntp_initialized;
static bool s_mdns_initialized;
static bool s_wifi_initialized;
static sys_config_t s_active_config;
static bool s_has_active_config;
static bool s_reconnect_immediately;

static void net_status_set_state(net_task_state_t state)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status.state = state;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void net_status_set_bits(uint32_t bits)
{
    xEventGroupSetBits(s_state_events, bits);
}

static void net_status_clear_bits(uint32_t bits)
{
    xEventGroupClearBits(s_state_events, bits);
}

static void net_queue_event(const net_internal_event_t *event)
{
    if (s_event_queue && xQueueSend(s_event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "network event queue full, event=%d", event->id);
    }
}

static void net_wifi_event_callback(const wifi_sta_event_t *wifi_event,
                                    void *context)
{
    net_internal_event_t event = {
        .id = NET_INTERNAL_WIFI_EVENT,
        .wifi = *wifi_event,
    };
    net_queue_event(&event);
}

static void net_ntp_sync_callback(time_t sync_time, void *context)
{
    net_internal_event_t event = {
        .id = NET_INTERNAL_TIME_SYNCED,
        .sync_time = sync_time,
    };
    net_queue_event(&event);
}

static esp_err_t net_ntp_init(void)
{
    if (s_ntp_initialized) {
        return ESP_OK;
    }
    esp_err_t err = ntp_client_init(NET_NTP_SERVER, NET_TIMEZONE,
                                    net_ntp_sync_callback, NULL);
    if (err == ESP_OK) {
        s_ntp_initialized = true;
    }
    return err;
}

static esp_err_t net_mdns_init(void)
{
    if (s_mdns_initialized) {
        return ESP_OK;
    }
    if (!s_has_active_config) {
        return ESP_ERR_INVALID_STATE;
    }

    char hostname[sizeof(s_status.mdns_hostname)];
    char instance_name[64];
    if (s_active_config.mdns_hostname[0] != '\0') {
        strlcpy(hostname, s_active_config.mdns_hostname, sizeof(hostname));
    } else {
        snprintf(hostname, sizeof(hostname), "ac-remote-%u",
                 s_active_config.device_id);
    }
    strlcpy(instance_name,
            s_active_config.device_name[0] != '\0'
                ? s_active_config.device_name : "AC Remote",
            sizeof(instance_name));

    esp_err_t err = network_mdns_init(hostname, instance_name);
    if (err != ESP_OK) {
        return err;
    }

    taskENTER_CRITICAL(&s_status_lock);
    strlcpy(s_status.mdns_hostname, hostname,
            sizeof(s_status.mdns_hostname));
    taskEXIT_CRITICAL(&s_status_lock);
    s_mdns_initialized = true;
    net_status_set_bits(NET_STATE_MDNS_READY);
    return ESP_OK;
}

static uint32_t net_next_reconnect_delay(uint32_t reconnect_count)
{
    uint32_t shift = reconnect_count > 5 ? 5 : reconnect_count;
    uint32_t delay = NET_RECONNECT_INITIAL_MS << shift;
    return delay > NET_RECONNECT_MAX_MS ? NET_RECONNECT_MAX_MS : delay;
}

static void net_schedule_reconnect(uint32_t *delay_ms)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status.reconnect_count++;
    *delay_ms = net_next_reconnect_delay(s_status.reconnect_count - 1);
    s_status.reconnect_delay_ms = *delay_ms;
    s_status.state = NET_TASK_STATE_BACKOFF;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void net_connect_now(uint32_t *delay_ms)
{
    *delay_ms = 0;
    taskENTER_CRITICAL(&s_status_lock);
    s_status.state = NET_TASK_STATE_CONNECTING;
    s_status.reconnect_delay_ms = 0;
    taskEXIT_CRITICAL(&s_status_lock);
    esp_err_t err = network_wifi_sta_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
        net_schedule_reconnect(delay_ms);
    }
}

static bool net_credentials_changed(const sys_config_t *config)
{
    if (config->wifi_ssid[0] == '\0') {
        return s_has_active_config;
    }
    return !s_has_active_config ||
           strcmp(s_active_config.wifi_ssid, config->wifi_ssid) != 0 ||
           strcmp(s_active_config.wifi_password, config->wifi_password) != 0;
}

static bool net_mdns_identity_changed(const sys_config_t *config)
{
    if (!s_has_active_config) {
        return false;
    }
    if (strcmp(s_active_config.mdns_hostname, config->mdns_hostname) != 0 ||
        strcmp(s_active_config.device_name, config->device_name) != 0) {
        return true;
    }
    return config->mdns_hostname[0] == '\0' &&
           s_active_config.device_id != config->device_id;
}

static esp_err_t net_apply_config(const sys_config_t *config, uint32_t *delay_ms)
{
    if (config->wifi_ssid[0] == '\0') {
        if (s_wifi_initialized && s_has_active_config) {
            s_reconnect_immediately = false;
            network_wifi_sta_disconnect();
            net_status_clear_bits(NET_STATE_WIFI_CONNECTED |
                                  NET_STATE_IPV4_READY);
        }
        network_mdns_deinit();
        s_mdns_initialized = false;
        net_status_clear_bits(NET_STATE_MDNS_READY);
        s_has_active_config = false;
        taskENTER_CRITICAL(&s_status_lock);
        s_status.ssid[0] = '\0';
        s_status.mdns_hostname[0] = '\0';
        taskEXIT_CRITICAL(&s_status_lock);
        net_status_set_state(NET_TASK_STATE_WAITING_CONFIG);
        return ESP_ERR_NOT_FOUND;
    }

    bool mdns_identity_changed = net_mdns_identity_changed(config);
    esp_err_t err;
    bool newly_initialized = false;
    if (!s_wifi_initialized) {
        err = network_wifi_sta_init(config->wifi_ssid, config->wifi_password,
                                    net_wifi_event_callback, NULL);
        if (err != ESP_OK) {
            return err;
        }
        s_wifi_initialized = true;
        newly_initialized = true;
        err = net_ntp_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NTP initialization failed: %s", esp_err_to_name(err));
        }
    } else {
        s_reconnect_immediately = true;
        err = network_wifi_sta_disconnect();
        if (err != ESP_OK) {
            s_reconnect_immediately = false;
        }
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(err));
        }
        err = network_wifi_sta_set_credentials(config->wifi_ssid,
                                               config->wifi_password);
        if (err != ESP_OK) {
            return err;
        }
    }

    taskENTER_CRITICAL(&s_status_lock);
    strlcpy(s_status.ssid, config->wifi_ssid, sizeof(s_status.ssid));
    s_status.reconnect_count = 0;
    s_status.reconnect_delay_ms = 0;
    taskEXIT_CRITICAL(&s_status_lock);
    s_active_config = *config;
    s_has_active_config = true;
    if (mdns_identity_changed) {
        network_mdns_deinit();
        s_mdns_initialized = false;
        net_status_clear_bits(NET_STATE_MDNS_READY);
        taskENTER_CRITICAL(&s_status_lock);
        s_status.mdns_hostname[0] = '\0';
        taskEXIT_CRITICAL(&s_status_lock);
    }
    *delay_ms = 0;
    if (!newly_initialized && !s_reconnect_immediately) {
        net_connect_now(delay_ms);
    }
    return ESP_OK;
}

static void net_handle_wifi_event(const wifi_sta_event_t *event,
                                  uint32_t *reconnect_delay_ms)
{
    switch (event->id) {
    case WIFI_STA_EVENT_STARTED:
        net_connect_now(reconnect_delay_ms);
        break;
    case WIFI_STA_EVENT_CONNECTED:
        net_status_set_bits(NET_STATE_WIFI_CONNECTED);
        net_status_set_state(NET_TASK_STATE_WAITING_IP);
        break;
    case WIFI_STA_EVENT_DISCONNECTED:
        net_status_clear_bits(NET_STATE_WIFI_CONNECTED | NET_STATE_IPV4_READY);
        taskENTER_CRITICAL(&s_status_lock);
        memset(&s_status.ip_info, 0, sizeof(s_status.ip_info));
        s_status.disconnect_reason = event->data.disconnected.reason;
        s_status.disconnect_rssi = event->data.disconnected.rssi;
        taskEXIT_CRITICAL(&s_status_lock);
        if (s_reconnect_immediately) {
            s_reconnect_immediately = false;
            net_connect_now(reconnect_delay_ms);
        } else if (s_has_active_config) {
            net_schedule_reconnect(reconnect_delay_ms);
            ESP_LOGW(TAG, "WiFi disconnected, reason=%u, retry in %lu ms",
                     event->data.disconnected.reason,
                     (unsigned long)*reconnect_delay_ms);
        } else {
            net_status_set_state(NET_TASK_STATE_WAITING_CONFIG);
        }
        break;
    case WIFI_STA_EVENT_GOT_IP:
        taskENTER_CRITICAL(&s_status_lock);
        s_status.ip_info = event->data.ip_info;
        s_status.reconnect_count = 0;
        s_status.reconnect_delay_ms = 0;
        s_status.state = NET_TASK_STATE_ONLINE;
        taskEXIT_CRITICAL(&s_status_lock);
        net_status_set_bits(NET_STATE_WIFI_CONNECTED | NET_STATE_IPV4_READY);
        *reconnect_delay_ms = 0;
        if (!s_ntp_initialized) {
            esp_err_t err = net_ntp_init();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "NTP initialization retry failed: %s",
                         esp_err_to_name(err));
            }
        }
        if (s_ntp_initialized) {
            esp_err_t err = ntp_client_start();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "NTP start failed: %s", esp_err_to_name(err));
            }
        }
        if (!s_mdns_initialized) {
            esp_err_t err = net_mdns_init();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "mDNS initialization failed: %s",
                         esp_err_to_name(err));
            }
        }
        break;
    case WIFI_STA_EVENT_LOST_IP:
        net_status_clear_bits(NET_STATE_IPV4_READY);
        taskENTER_CRITICAL(&s_status_lock);
        memset(&s_status.ip_info, 0, sizeof(s_status.ip_info));
        taskEXIT_CRITICAL(&s_status_lock);
        if (xEventGroupGetBits(s_state_events) & NET_STATE_WIFI_CONNECTED) {
            net_status_set_state(NET_TASK_STATE_WAITING_IP);
        }
        break;
    }
}

static void net_task_entry(void *parameter)
{
    uint32_t reconnect_delay_ms = 0;
    sys_config_t config = {0};
    TickType_t last_config_check = 0;

    if (sys_config_get(&config) == ESP_OK) {
        esp_err_t err = net_apply_config(&config, &reconnect_delay_ms);
        if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "network start failed: %s", esp_err_to_name(err));
            net_schedule_reconnect(&reconnect_delay_ms);
        }
    }

    for (;;) {
        TickType_t wait_ticks = reconnect_delay_ms > 0
                              ? pdMS_TO_TICKS(reconnect_delay_ms)
                              : pdMS_TO_TICKS(NET_CONFIG_CHECK_MS);
        net_internal_event_t event;
        if (xQueueReceive(s_event_queue, &event, wait_ticks) == pdTRUE) {
            if (event.id == NET_INTERNAL_WIFI_EVENT) {
                net_handle_wifi_event(&event.wifi, &reconnect_delay_ms);
            } else if (event.id == NET_INTERNAL_TIME_SYNCED) {
                taskENTER_CRITICAL(&s_status_lock);
                s_status.last_sync_time = event.sync_time;
                taskEXIT_CRITICAL(&s_status_lock);
                net_status_set_bits(NET_STATE_TIME_SYNCED);
                ESP_LOGI(TAG, "system time synchronized");
            } else if (event.id == NET_INTERNAL_RECONNECT && s_wifi_initialized) {
                s_reconnect_immediately = true;
                esp_err_t err = network_wifi_sta_disconnect();
                if (err == ESP_ERR_WIFI_NOT_CONNECT) {
                    s_reconnect_immediately = false;
                    net_connect_now(&reconnect_delay_ms);
                } else if (err != ESP_OK) {
                    s_reconnect_immediately = false;
                    ESP_LOGW(TAG, "WiFi reconnect request failed: %s",
                             esp_err_to_name(err));
                }
            } else if (event.id == NET_INTERNAL_SYNC_TIME) {
                if (!(xEventGroupGetBits(s_state_events) & NET_STATE_IPV4_READY)) {
                    ESP_LOGW(TAG, "cannot synchronize time without IPv4");
                } else {
                    esp_err_t err = s_ntp_initialized ? ESP_OK : net_ntp_init();
                    if (err == ESP_OK) {
                        err = ntp_client_start();
                    }
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "manual NTP sync failed: %s",
                                 esp_err_to_name(err));
                    }
                }
            }
            continue;
        }

        if (reconnect_delay_ms > 0 && s_wifi_initialized) {
            net_connect_now(&reconnect_delay_ms);
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_config_check >= pdMS_TO_TICKS(NET_CONFIG_CHECK_MS)) {
            last_config_check = now;
            sys_config_t latest;
            if (sys_config_get(&latest) == ESP_OK) {
                if (net_credentials_changed(&latest)) {
                    esp_err_t err = net_apply_config(&latest,
                                                     &reconnect_delay_ms);
                    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
                        ESP_LOGW(TAG, "apply network config failed: %s",
                                 esp_err_to_name(err));
                    }
                } else if (net_mdns_identity_changed(&latest)) {
                    s_active_config = latest;
                    network_mdns_deinit();
                    s_mdns_initialized = false;
                    net_status_clear_bits(NET_STATE_MDNS_READY);
                    taskENTER_CRITICAL(&s_status_lock);
                    s_status.mdns_hostname[0] = '\0';
                    taskEXIT_CRITICAL(&s_status_lock);
                    if (xEventGroupGetBits(s_state_events) &
                        NET_STATE_IPV4_READY) {
                        esp_err_t err = net_mdns_init();
                        if (err != ESP_OK) {
                            ESP_LOGW(TAG, "refresh mDNS identity failed: %s",
                                     esp_err_to_name(err));
                        }
                    }
                }
            }
        }
    }
}

esp_err_t net_task_start(void)
{
    if (s_net_task_handle) {
        return ESP_OK;
    }

    if (!s_state_events) {
        s_state_events = xEventGroupCreate();
    }
    if (!s_event_queue) {
        s_event_queue = xQueueCreate(NET_EVENT_QUEUE_LENGTH,
                                     sizeof(net_internal_event_t));
    }
    if (!s_state_events || !s_event_queue) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = NET_TASK_STATE_WAITING_CONFIG;
    BaseType_t result = xTaskCreate(net_task_entry, "net_task", 4096,
                                    NULL, 5, &s_net_task_handle);
    return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t net_task_stop(void)
{
    if (!s_net_task_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    TaskHandle_t task = s_net_task_handle;
    s_net_task_handle = NULL;
    vTaskDelete(task);
    ntp_client_deinit();
    s_ntp_initialized = false;
    network_mdns_deinit();
    s_mdns_initialized = false;
    network_wifi_sta_stop();
    s_wifi_initialized = false;
    s_has_active_config = false;
    s_reconnect_immediately = false;
    xQueueReset(s_event_queue);
    net_status_clear_bits(NET_STATE_ALL);
    net_status_set_state(NET_TASK_STATE_STOPPED);
    return ESP_OK;
}

bool net_task_is_running(void)
{
    return s_net_task_handle != NULL;
}

esp_err_t net_get_status(net_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_state_events) {
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_status_lock);
    *status = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
    status->state_bits = xEventGroupGetBits(s_state_events) & NET_STATE_ALL;
    return ESP_OK;
}

esp_err_t net_wait(uint32_t required_states, TickType_t timeout)
{
    if (!s_state_events) {
        return ESP_ERR_INVALID_STATE;
    }
    if (required_states == 0 || (required_states & ~NET_STATE_ALL) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    EventBits_t bits = xEventGroupWaitBits(s_state_events, required_states,
                                           pdFALSE, pdTRUE, timeout);
    return (bits & required_states) == required_states ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t net_request_reconnect(void)
{
    if (!s_net_task_handle || !s_event_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    net_internal_event_t event = {.id = NET_INTERNAL_RECONNECT};
    return xQueueSend(s_event_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t net_request_time_sync(void)
{
    if (!s_net_task_handle || !s_event_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!(xEventGroupGetBits(s_state_events) & NET_STATE_IPV4_READY)) {
        return ESP_ERR_INVALID_STATE;
    }
    net_internal_event_t event = {.id = NET_INTERNAL_SYNC_TIME};
    return xQueueSend(s_event_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t net_time_get_epoch(time_t *result)
{
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    net_status_t status;
    esp_err_t err = net_get_status(&status);
    if (err != ESP_OK) {
        return err;
    }
    if ((status.state_bits & NET_STATE_TIME_SYNCED) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ntp_client_get_epoch(result);
}

esp_err_t net_time_get_local(struct tm *result)
{
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    net_status_t status;
    esp_err_t err = net_get_status(&status);
    if (err != ESP_OK) {
        return err;
    }
    if ((status.state_bits & NET_STATE_TIME_SYNCED) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ntp_client_get_local(result);
}

esp_err_t net_time_format(char *buffer, size_t size, const char *format)
{
    if (!buffer || size == 0 || !format) {
        return ESP_ERR_INVALID_ARG;
    }
    net_status_t status;
    esp_err_t err = net_get_status(&status);
    if (err != ESP_OK) {
        return err;
    }
    if ((status.state_bits & NET_STATE_TIME_SYNCED) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ntp_client_format_time(buffer, size, format);
}

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mdns_responder.h"
#include <stdbool.h>
#include <string.h>
#include "mdns.h"

#define MDNS_MAX_SERVICES 8
#define MDNS_SERVICE_NAME_LEN 16

typedef struct {
    bool used;
    char type[MDNS_SERVICE_NAME_LEN];
    char protocol[MDNS_SERVICE_NAME_LEN];
    uint16_t port;
} mdns_service_entry_t;

static bool s_initialized;
static mdns_service_entry_t s_services[MDNS_MAX_SERVICES];

static mdns_service_entry_t *find_service(const char *service_type,
                                          const char *protocol)
{
    for (size_t index = 0; index < MDNS_MAX_SERVICES; index++) {
        if (s_services[index].used &&
            strcmp(s_services[index].type, service_type) == 0 &&
            strcmp(s_services[index].protocol, protocol) == 0) {
            return &s_services[index];
        }
    }
    return NULL;
}

esp_err_t network_mdns_init(const char *hostname, const char *instance_name)
{
    if (!hostname || hostname[0] == '\0' ||
        !instance_name || instance_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        return err;
    }
    err = mdns_hostname_set(hostname);
    if (err == ESP_OK) {
        err = mdns_instance_name_set(instance_name);
    }
    if (err != ESP_OK) {
        mdns_free();
        return err;
    }

    s_initialized = true;
    for (size_t index = 0; index < MDNS_MAX_SERVICES; index++) {
        if (!s_services[index].used) {
            continue;
        }
        err = mdns_service_add(NULL, s_services[index].type,
                               s_services[index].protocol,
                               s_services[index].port, NULL, 0);
        if (err != ESP_OK) {
            mdns_free();
            s_initialized = false;
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t network_mdns_add_service(const char *service_type,
                                   const char *protocol, uint16_t port)
{
    if (!service_type || service_type[0] == '\0' ||
        !protocol || protocol[0] == '\0' || port == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(service_type) >= MDNS_SERVICE_NAME_LEN ||
        strlen(protocol) >= MDNS_SERVICE_NAME_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    mdns_service_entry_t *entry = find_service(service_type, protocol);
    if (entry) {
        if (entry->port == port) {
            return ESP_OK;
        }
        if (s_initialized) {
            esp_err_t err = mdns_service_remove(service_type, protocol);
            if (err != ESP_OK) {
                return err;
            }
        }
    } else {
        for (size_t index = 0; index < MDNS_MAX_SERVICES; index++) {
            if (!s_services[index].used) {
                entry = &s_services[index];
                break;
            }
        }
        if (!entry) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_initialized) {
        esp_err_t err = mdns_service_add(NULL, service_type, protocol, port,
                                         NULL, 0);
        if (err != ESP_OK) {
            return err;
        }
    }
    entry->used = true;
    entry->port = port;
    strlcpy(entry->type, service_type, sizeof(entry->type));
    strlcpy(entry->protocol, protocol, sizeof(entry->protocol));
    return ESP_OK;
}

esp_err_t network_mdns_remove_service(const char *service_type,
                                      const char *protocol)
{
    if (!service_type || service_type[0] == '\0' ||
        !protocol || protocol[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    mdns_service_entry_t *entry = find_service(service_type, protocol);
    if (!entry) {
        return ESP_ERR_NOT_FOUND;
    }
    if (s_initialized) {
        esp_err_t err = mdns_service_remove(service_type, protocol);
        if (err != ESP_OK) {
            return err;
        }
    }
    memset(entry, 0, sizeof(*entry));
    return ESP_OK;
}

void network_mdns_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    mdns_free();
    s_initialized = false;
}

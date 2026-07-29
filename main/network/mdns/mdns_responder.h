/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MDNS_RESPONDER_H
#define MDNS_RESPONDER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t network_mdns_init(const char *hostname, const char *instance_name);
esp_err_t network_mdns_add_service(const char *service_type,
                                   const char *protocol, uint16_t port);
esp_err_t network_mdns_remove_service(const char *service_type,
                                      const char *protocol);
void network_mdns_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // MDNS_RESPONDER_H

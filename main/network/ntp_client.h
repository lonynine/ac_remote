/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <stddef.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ntp_client_sync_cb_t)(time_t sync_time, void *context);

esp_err_t ntp_client_init(const char *server, const char *timezone,
                          ntp_client_sync_cb_t callback, void *context);
esp_err_t ntp_client_start(void);
void ntp_client_deinit(void);

esp_err_t ntp_client_get_epoch(time_t *result);
esp_err_t ntp_client_get_local(struct tm *result);
esp_err_t ntp_client_format_time(char *buffer, size_t size,
                                 const char *format);

#ifdef __cplusplus
}
#endif

#endif // NTP_CLIENT_H

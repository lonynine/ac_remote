/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "web_ui.h"

#include <stddef.h>

extern const char control_html_start[] asm("_binary_control_html_start");
extern const char control_html_end[] asm("_binary_control_html_end");
extern const char favicon_svg_start[] asm("_binary_favicon_svg_start");
extern const char favicon_svg_end[] asm("_binary_favicon_svg_end");

esp_err_t web_ui_handler(httpd_req_t *request)
{
    size_t page_size = (size_t)(control_html_end - control_html_start);
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(request, control_html_start, page_size);
}

esp_err_t web_favicon_handler(httpd_req_t *request)
{
    size_t icon_size = (size_t)(favicon_svg_end - favicon_svg_start);
    httpd_resp_set_type(request, "image/svg+xml");
    httpd_resp_set_hdr(request, "Cache-Control", "public, max-age=86400");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(request, favicon_svg_start, icon_size);
}

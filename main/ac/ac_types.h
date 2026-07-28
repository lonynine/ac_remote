/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AC_TYPES_H
#define AC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AC_BRAND_HAIER = 0,
    AC_BRAND_GREE  = 1,
    AC_BRAND_MIDEA = 2,
    AC_BRAND_AUX   = 3,
} ac_brand_t;

typedef enum {
    AC_MODE_AUTO = 0,
    AC_MODE_COOL = 1,
    AC_MODE_DRY  = 2,
    AC_MODE_FAN  = 3,
    AC_MODE_HEAT = 4,
} ac_mode_t;

typedef enum {
    AC_FAN_AUTO = 0,
    AC_FAN_LOW  = 1,
    AC_FAN_MED  = 2,
    AC_FAN_HIGH = 3,
} ac_fan_t;

typedef struct {
    ac_brand_t brand;
    bool power;
    ac_mode_t mode;
    uint8_t temp;
    ac_fan_t fan;
    bool swing;
    bool light;
} ac_remote_cmd_t;

typedef ac_mode_t gree_mode_t;
typedef ac_fan_t  gree_fan_t;
typedef ac_mode_t haier_mode_t;
typedef ac_fan_t  haier_fan_t;

#define GREE_MODE_AUTO AC_MODE_AUTO
#define GREE_MODE_COOL AC_MODE_COOL
#define GREE_MODE_DRY  AC_MODE_DRY
#define GREE_MODE_FAN  AC_MODE_FAN
#define GREE_MODE_HEAT AC_MODE_HEAT

#define GREE_FAN_AUTO  AC_FAN_AUTO
#define GREE_FAN_LOW   AC_FAN_LOW
#define GREE_FAN_MED   AC_FAN_MED
#define GREE_FAN_HIGH  AC_FAN_HIGH

#ifdef __cplusplus
}
#endif

#endif // AC_TYPES_H

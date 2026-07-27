/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 温湿度传感器数据结构体
 */
typedef struct {
    float temperature;  // 温度 (℃)
    float humidity;     // 湿度 (%RH)
    bool valid;         // 数据是否有效
} sensor_data_t;

/**
 * @brief 初始化温湿度采集任务
 */
esp_err_t sensor_task_init(void);

/**
 * @brief 启动温湿度采集任务
 */
esp_err_t sensor_task_start(void);

/**
 * @brief 停止温湿度采集任务
 */
esp_err_t sensor_task_stop(void);

/**
 * @brief 查询温湿度采集任务运行状态
 */
bool sensor_task_is_running(void);

/**
 * @brief 获取最新的温湿度数据
 * @param[out] temp 温度指针 (可为 NULL)
 * @param[out] humi 湿度指针 (可为 NULL)
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 任务未准备好
 */
esp_err_t sensor_task_get_data(float *temp, float *humi);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_TASK_H

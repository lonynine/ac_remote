/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "control_task.h"
#include "ir_remote.h"
#include "ac_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "ctrl";

static QueueHandle_t s_control_queue = NULL;
static TaskHandle_t s_control_task_handle = NULL;

static void control_task_proc(void *pvParameters)
{
    ESP_LOGI(TAG, "红外控制后台独占任务启动，开始监听队列指令...");

    control_msg_t msg;
    while (1) {
        // 无限等待队列中的发送指令 (硬件资源独占保护)
        if (xQueueReceive(s_control_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "📬 [队列接收消息] 开始处理红外发波请求 (消息类型: %d)...", msg.type);

            if (msg.type == CONTROL_MSG_TYPE_CMD) {
                // 独占驱动硬件发送
                esp_err_t err = ir_remote_send_cmd(&msg.cmd);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "红外发波失败，状态缓存不更新: %s", esp_err_to_name(err));
                    continue;
                }

                // 更新全局状态缓存
                ac_state_t state;
                if (ac_state_get(&state) == ESP_OK) {
                    state.power = msg.cmd.power;
                    state.mode = msg.cmd.mode;
                    state.temp = msg.cmd.temp;
                    state.fan = msg.cmd.fan;
                    ac_state_set(&state);
                }
            } else if (msg.type == CONTROL_MSG_TYPE_EMIT) {
                // 独占驱动硬件重发学到的波形
                ir_remote_learn_emit();
            }
        }
    }
}

esp_err_t control_task_init(void)
{
    if (s_control_queue == NULL) {
        s_control_queue = xQueueCreate(10, sizeof(control_msg_t));
        if (s_control_queue == NULL) {
            ESP_LOGE(TAG, "创建控制队列失败!");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t control_task_start(void)
{
    if (s_control_task_handle != NULL) {
        return ESP_OK; // 已启动
    }

    if (s_control_queue == NULL) {
        esp_err_t err = control_task_init();
        if (err != ESP_OK) return err;
    }

    BaseType_t ret = xTaskCreate(control_task_proc, "control_task", 4096, NULL, 5, &s_control_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 control_task 任务失败!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t control_task_stop(void)
{
    if (s_control_task_handle != NULL) {
        vTaskDelete(s_control_task_handle);
        s_control_task_handle = NULL;
    }
    return ESP_OK;
}

bool control_task_is_running(void)
{
    return s_control_task_handle != NULL;
}

esp_err_t control_task_post_cmd(const ac_remote_cmd_t *cmd)
{
    if (s_control_queue == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    control_msg_t msg = {0};
    msg.type = CONTROL_MSG_TYPE_CMD;
    msg.cmd = *cmd;

    if (xQueueSend(s_control_queue, &msg, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "控制队列已满，推入发波指令失败!");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t control_task_post_emit(void)
{
    if (s_control_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    control_msg_t msg = {0};
    msg.type = CONTROL_MSG_TYPE_EMIT;

    if (xQueueSend(s_control_queue, &msg, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "控制队列已满，推入重发指令失败!");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

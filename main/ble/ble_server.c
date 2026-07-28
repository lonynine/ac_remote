/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_server.h"
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "config.h"

static const char *TAG = "ble";

#define PROFILE_NUM                 1
#define PROFILE_APP_ID              0
#define GATTS_SERVICE_UUID_TEST     0x00FF
#define GATTS_CHAR_UUID_CONFIG      0xFF01
#define GATTS_NUM_HANDLE_TEST       4

static bool s_is_connected = false;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_char_handle = 0;
static uint16_t s_conn_id = 0;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "BLE 广播启动成功，正在等待手机客户端连接...");
        } else {
            ESP_LOGE(TAG, "BLE 广播启动失败");
        }
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        s_gatts_if = gatts_if;
        esp_gatt_srvc_id_t service_id = {
            .is_primary = true,
            .id = {
                .inst_id = 0x00,
                .uuid = {
                    .len = ESP_UUID_LEN_16,
                    .uuid = {.uuid16 = GATTS_SERVICE_UUID_TEST},
                },
            },
        };
        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_TEST);
        break;
    }
    case ESP_GATTS_CREATE_EVT: {
        uint16_t service_handle = param->create.service_handle;
        esp_bt_uuid_t char_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = GATTS_CHAR_UUID_CONFIG},
        };
        esp_attr_value_t char_val = {
            .attr_max_len = 512,
            .attr_len     = 0,
            .attr_value   = NULL,
        };
        esp_ble_gatts_add_char(service_handle, &char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                               &char_val, NULL);
        esp_ble_gatts_start_service(service_handle);
        break;
    }
    case ESP_GATTS_ADD_CHAR_EVT:
        s_char_handle = param->add_char.attr_handle;
        ESP_LOGI(TAG, "GATT 特征值创建完成，句柄: %d", s_char_handle);
        break;
    case ESP_GATTS_CONNECT_EVT:
        s_is_connected = true;
        s_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "手机 App 已成功通过 BLE 连接到设备！");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        s_is_connected = false;
        s_conn_id = 0;
        ESP_LOGI(TAG, "BLE 手机断开连接，重新启动广播...");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "收到手机 App 写入的参数数据 (长度: %d)", param->write.len);
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gatts_profile_event_handler(event, gatts_if, param);
        } else {
            ESP_LOGE(TAG, "GATTS 注册失败, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    } else {
        gatts_profile_event_handler(event, gatts_if, param);
    }
}

esp_err_t ble_server_init(const char *device_name)
{
    const char *name = (device_name && strlen(device_name) > 0) ? device_name : "ESP32S3-Control";

    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "释放 Classic BT 内存失败: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "初始化 BT 控制器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "使能 BLE 模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "初始化 Bluedroid 协议栈失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "使能 Bluedroid 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 GATTS 回调失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 GAP 回调失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 GATTS app 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_ble_gap_set_device_name(name);

    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    esp_ble_gap_config_adv_data(&adv_data);

    ESP_LOGI(TAG, "BLE 蓝牙初始化成功，设备广播名: %s", name);
    return ESP_OK;
}

bool ble_server_is_connected(void)
{
    return s_is_connected;
}

esp_err_t ble_server_stop(void)
{
    if (s_is_connected) {
        esp_ble_gatts_close(s_gatts_if, s_conn_id);
    }
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    return ESP_OK;
}

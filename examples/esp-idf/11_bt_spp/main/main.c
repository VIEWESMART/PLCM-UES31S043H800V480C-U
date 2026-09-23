#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "bsp_ble_uart";
static const char *DEVICE_NAME = "S31-BSP-UART";

/* Nordic UART Service, used by Serial Bluetooth Terminal (Bluetooth LE). */
static const uint8_t NUS_SVC[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};
static const uint8_t NUS_RX[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
};
static const uint8_t NUS_TX[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
};

static const uint16_t UUID_PRI_SVC = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECL = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_CCCD = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t CHAR_PROP_WRITE = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t CHAR_PROP_NOTIFY = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t CCCD_OFF[2] = {0x00, 0x00};

enum {
    IDX_SVC,
    IDX_RX_DECL,
    IDX_RX_VAL,
    IDX_TX_DECL,
    IDX_TX_VAL,
    IDX_TX_CCCD,
    IDX_NB,
};

static uint16_t s_handle[IDX_NB];
static uint16_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0xFFFF;
static bool s_connected;
static bool s_notify;
static uint8_t s_rx_buf[128];
static uint8_t s_tx_buf[128];

static const esp_gatts_attr_db_t s_gatt_db[IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP},
                 {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRI_SVC, ESP_GATT_PERM_READ,
                  sizeof(NUS_SVC), sizeof(NUS_SVC), (uint8_t *)NUS_SVC}},
    [IDX_RX_DECL] = {{ESP_GATT_AUTO_RSP},
                     {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECL, ESP_GATT_PERM_READ,
                      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&CHAR_PROP_WRITE}},
    [IDX_RX_VAL] = {{ESP_GATT_AUTO_RSP},
                    {ESP_UUID_LEN_128, (uint8_t *)NUS_RX, ESP_GATT_PERM_WRITE,
                     sizeof(s_rx_buf), 0, s_rx_buf}},
    [IDX_TX_DECL] = {{ESP_GATT_AUTO_RSP},
                     {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECL, ESP_GATT_PERM_READ,
                      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&CHAR_PROP_NOTIFY}},
    [IDX_TX_VAL] = {{ESP_GATT_AUTO_RSP},
                    {ESP_UUID_LEN_128, (uint8_t *)NUS_TX, ESP_GATT_PERM_READ,
                     sizeof(s_tx_buf), 0, s_tx_buf}},
    [IDX_TX_CCCD] = {{ESP_GATT_AUTO_RSP},
                     {ESP_UUID_LEN_16, (uint8_t *)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                      sizeof(CCCD_OFF), sizeof(CCCD_OFF), (uint8_t *)CCCD_OFF}},
};

static uint8_t s_adv_raw[] = {
    0x02, 0x01, 0x06,
    0x11, 0x07,
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
};

static uint8_t s_scan_rsp[] = {
    0x0D, 0x09, 'S', '3', '1', '-', 'B', 'S', 'P', '-', 'U', 'A', 'R', 'T',
};

static void start_adv(void)
{
    esp_ble_adv_params_t p = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    esp_ble_gap_start_advertising(&p);
}

static void echo_notify(const uint8_t *data, uint16_t len)
{
    if (!s_connected || s_gatts_if == ESP_GATT_IF_NONE || s_conn_id == 0xFFFF) {
        return;
    }
    if (len > sizeof(s_tx_buf)) {
        len = sizeof(s_tx_buf);
    }
    memcpy(s_tx_buf, data, len);
    esp_err_t err = esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_handle[IDX_TX_VAL],
                                                len, s_tx_buf, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "tx fail %s (open notify in the app if the phone shows nothing)",
                 esp_err_to_name(err));
    }
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        esp_ble_gap_config_scan_rsp_data_raw(s_scan_rsp, sizeof(s_scan_rsp));
    } else if (event == ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT) {
        start_adv();
    } else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        ESP_LOGI(TAG, "advertising %s — use Serial Bluetooth Terminal, Bluetooth LE", DEVICE_NAME);
    }
}

static void gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        s_gatts_if = gatts_if;
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gatts_create_attr_tab(s_gatt_db, gatts_if, IDX_NB, 0);
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK) {
            memcpy(s_handle, param->add_attr_tab.handles, sizeof(s_handle));
            esp_ble_gatts_start_service(s_handle[IDX_SVC]);
            esp_ble_gap_config_adv_data_raw(s_adv_raw, sizeof(s_adv_raw));
        } else {
            ESP_LOGE(TAG, "attr tab fail %d", param->add_attr_tab.status);
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        s_connected = true;
        s_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "BLE connected, board will send hello");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        s_connected = false;
        s_notify = false;
        s_conn_id = 0xFFFF;
        ESP_LOGI(TAG, "BLE disconnect reason=0x%x, advertising again", param->disconnect.reason);
        start_adv();
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == s_handle[IDX_TX_CCCD] && param->write.len >= 2) {
            s_notify = (param->write.value[0] != 0);
            ESP_LOGI(TAG, "notify %s", s_notify ? "on" : "off");
            if (s_notify) {
                const char *ready = "S31-BSP-UART echo ready\n";
                echo_notify((const uint8_t *)ready, (uint16_t)strlen(ready));
                echo_notify((const uint8_t *)"hello\n", 6);
            }
        } else if (param->write.handle == s_handle[IDX_RX_VAL] && param->write.len > 0) {
            ESP_LOGI(TAG, "rx %d bytes", param->write.len);
            if (param->write.len < 128) {
                ESP_LOG_BUFFER_CHAR(TAG, param->write.value, param->write.len);
            }
            echo_notify(param->write.value, param->write.len);
        }
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                        ESP_GATT_OK, NULL);
        }
        break;
    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_cb));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(247));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    while (1) {
        if (s_connected) {
            ESP_LOGI(TAG, "tx hello");
            echo_notify((const uint8_t *)"hello\n", 6);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

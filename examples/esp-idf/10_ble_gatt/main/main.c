#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_hid_gap.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_BT_BLE_ENABLED
#include "esp_gatts_api.h"
#endif

static const char *TAG = "bsp_ble_hid";

static const char *BTN_NAMES[] = {"SW3", "SW4", "SW5", "SW6"};

static esp_hidd_dev_t *s_hid_dev;
static volatile bool s_hid_connected;

/* Official consumer-control map (report ID 3). */
static const unsigned char media_report_map[] = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03, 0x09, 0x02, 0xA1, 0x02,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0A, 0x15, 0x01, 0x25, 0x0A, 0x75, 0x04,
    0x95, 0x01, 0x81, 0x00, 0xC0, 0x05, 0x0C, 0x09, 0x86, 0x15, 0xFF, 0x25,
    0x01, 0x75, 0x02, 0x95, 0x01, 0x81, 0x46, 0x09, 0xE9, 0x09, 0xEA, 0x15,
    0x00, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02, 0x09, 0xE2, 0x09, 0x30, 0x09,
    0x83, 0x09, 0x81, 0x09, 0xB0, 0x09, 0xB1, 0x09, 0xB2, 0x09, 0xB3, 0x09,
    0xB4, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7, 0x15, 0x01, 0x25, 0x0C, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x00, 0x09, 0x80, 0xA1, 0x02, 0x05, 0x09, 0x19,
    0x01, 0x29, 0x03, 0x15, 0x01, 0x25, 0x03, 0x75, 0x02, 0x81, 0x00, 0xC0,
    0x81, 0x03, 0xC0,
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    { .data = media_report_map, .len = sizeof(media_report_map) },
};

static esp_hid_device_config_t s_hid_cfg = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "S31-BSP-HID",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

#define HID_CC_RPT_MUTE          1
#define HID_CC_RPT_PLAY          5
#define HID_CC_RPT_VOLUME_UP     0x40
#define HID_CC_RPT_VOLUME_DOWN   0x80
#define HID_CC_RPT_VOLUME_BITS   0x3F
#define HID_CC_RPT_BUTTON_BITS   0xF0
#define HID_RPT_ID_CC_IN         3
#define HID_CC_IN_RPT_LEN        2

#define HID_CONSUMER_VOLUME_UP   233
#define HID_CONSUMER_VOLUME_DOWN 234
#define HID_CONSUMER_MUTE        226
#define HID_CONSUMER_PLAY        176

static void send_consumer(uint8_t key_cmd, bool pressed)
{
    uint8_t buffer[HID_CC_IN_RPT_LEN] = {0, 0};
    if (s_hid_dev == NULL || !s_hid_connected) {
        return;
    }
    if (pressed) {
        switch (key_cmd) {
        case HID_CONSUMER_VOLUME_UP:
            buffer[0] = (buffer[0] & HID_CC_RPT_VOLUME_BITS) | HID_CC_RPT_VOLUME_UP;
            break;
        case HID_CONSUMER_VOLUME_DOWN:
            buffer[0] = (buffer[0] & HID_CC_RPT_VOLUME_BITS) | HID_CC_RPT_VOLUME_DOWN;
            break;
        case HID_CONSUMER_MUTE:
            buffer[1] = (buffer[1] & HID_CC_RPT_BUTTON_BITS) | HID_CC_RPT_MUTE;
            break;
        case HID_CONSUMER_PLAY:
            buffer[1] = (buffer[1] & HID_CC_RPT_BUTTON_BITS) | HID_CC_RPT_PLAY;
            break;
        default:
            break;
        }
    }
    esp_hidd_dev_input_set(s_hid_dev, 0, HID_RPT_ID_CC_IN, buffer, HID_CC_IN_RPT_LEN);
}

static uint8_t key_for_button(int idx)
{
    switch (idx) {
    case BSP_BUTTON_SW3:
        return HID_CONSUMER_VOLUME_DOWN;
    case BSP_BUTTON_SW4:
        return HID_CONSUMER_VOLUME_UP;
    case BSP_BUTTON_SW5:
        return HID_CONSUMER_MUTE;
    case BSP_BUTTON_SW6:
        return HID_CONSUMER_PLAY;
    default:
        return 0;
    }
}

static void button_cb(void *h, void *arg)
{
    int idx = (int)(intptr_t)arg;
    button_event_t ev = iot_button_get_event(h);
    bool pressed = (ev == BUTTON_PRESS_DOWN);
    uint8_t key = key_for_button(idx);

    if (ev != BUTTON_PRESS_DOWN && ev != BUTTON_PRESS_UP) {
        return;
    }
    ESP_LOGI(TAG, "%s %s key=%u connected=%d", BTN_NAMES[idx],
             pressed ? "down" : "up", key, (int)s_hid_connected);
    if (key) {
        send_consumer(key, pressed);
    }
}

static void hid_event_cb(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID start, advertising S31-BSP-HID");
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        s_hid_connected = true;
        ESP_LOGI(TAG, "HID connected, press SW3-SW6");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        s_hid_connected = false;
        ESP_LOGI(TAG, "HID disconnect: %s",
                 esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev),
                                               param->disconnect.reason));
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_STOP_EVENT:
        s_hid_connected = false;
        ESP_LOGI(TAG, "HID stop");
        break;
    default:
        break;
    }
}

/* Official GAP helper calls this after SMP auth; keys are driven by ADC, not a demo task. */
void ble_hid_task_start_up(void)
{
}

static void init_adc_keys(void)
{
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, button_cb, (void *)(intptr_t)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_UP, NULL, button_cb, (void *)(intptr_t)i);
    }
    ESP_LOGI(TAG, "SW3 vol-  SW4 vol+  SW5 mute  SW6 play");
}

void app_main(void)
{
#if HID_DEV_MODE == HIDD_IDLE_MODE
    ESP_LOGE(TAG, "enable CONFIG_BT_BLE_ENABLED");
    return;
#endif
    ESP_ERROR_CHECK(bsp_bt_nvs_init());
    init_adc_keys();

    ESP_LOGI(TAG, "HID GAP mode=%d", HID_DEV_MODE);
    ESP_ERROR_CHECK(esp_hid_gap_init(HID_DEV_MODE));
    ESP_ERROR_CHECK(esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, s_hid_cfg.device_name));
#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler));
#endif
    ESP_ERROR_CHECK(esp_hidd_dev_init(&s_hid_cfg, ESP_HID_TRANSPORT_BLE, hid_event_cb, &s_hid_dev));
    ESP_ERROR_CHECK(esp_hidd_dev_battery_set(s_hid_dev, 80));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

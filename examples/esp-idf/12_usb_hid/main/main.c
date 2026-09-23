#include "bsp/esp-bsp.h"
#include "class/hid/hid_device.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

static const char *TAG = "bsp_usb";
#define TUSB_DESC_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define TAP_MOVE_MAX_PX      18
#define TAP_MAX_MS           400
#define DBLCLICK_GAP_MS      450

static void hid_wait_ready(void)
{
    for (int i = 0; i < 50 && !tud_hid_ready(); i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void hid_mouse_move(int8_t dx, int8_t dy)
{
    if (!tud_mounted() || (dx == 0 && dy == 0)) {
        return;
    }
    hid_wait_ready();
    if (tud_hid_ready()) {
        tud_hid_mouse_report(0, 0, dx, dy, 0, 0);
    }
}

static void hid_left_clicks(int times)
{
    if (!tud_mounted()) {
        return;
    }
    for (int i = 0; i < times; i++) {
        hid_wait_ready();
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, MOUSE_BUTTON_LEFT, 0, 0, 0, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(12));
        hid_wait_ready();
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, 0, 0, 0, 0, 0);
        }
        if (i + 1 < times) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

static const uint8_t hid_report_descriptor[] = { TUD_HID_REPORT_DESC_MOUSE() };
static const char *hid_string_descriptor[] = {
    (char[]){0x09, 0x04}, "VIEWE", "S31 HID Touch", "123456", "HID Mouse",
};
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n)
{
    (void)i; (void)id; (void)t; (void)b; (void)n;
    return 0;
}
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n)
{
    (void)i; (void)id; (void)t; (void)b; (void)n;
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_usb_init());
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(bsp_display_new(NULL, &panel, NULL));
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp));

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = 5;
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
#endif
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB HID mouse: slide=move, tap=click, double-tap=select");

    bool down = false;
    bool mounted = false;
    uint16_t lx = 0, ly = 0;
    uint32_t move_px = 0;
    int64_t down_us = 0;
    int64_t last_tap_us = 0;
    while (1) {
        bool now = tud_mounted();
        if (now != mounted) {
            mounted = now;
            ESP_LOGI(TAG, "%s", mounted ? "USB mounted as HID mouse" : "USB unmounted");
        }
        if (esp_lcd_touch_read_data(tp) == ESP_OK) {
            esp_lcd_touch_point_data_t pts[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
            uint8_t n = 0;
            if (esp_lcd_touch_get_data(tp, pts, &n, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) == ESP_OK && n > 0) {
                if (down) {
                    int32_t x = (int32_t)pts[0].x - lx;
                    int32_t y = (int32_t)pts[0].y - ly;
                    int8_t dx = x > 127 ? 127 : x < -127 ? -127 : (int8_t)x;
                    int8_t dy = y > 127 ? 127 : y < -127 ? -127 : (int8_t)y;
                    uint32_t adx = dx < 0 ? (uint32_t)(-dx) : (uint32_t)dx;
                    uint32_t ady = dy < 0 ? (uint32_t)(-dy) : (uint32_t)dy;
                    move_px += adx + ady;
                    hid_mouse_move(dx, dy);
                } else {
                    move_px = 0;
                    down_us = esp_timer_get_time();
                    ESP_LOGI(TAG, "touch down x=%u y=%u usb=%d", pts[0].x, pts[0].y, (int)mounted);
                }
                lx = pts[0].x;
                ly = pts[0].y;
                down = true;
            } else if (down) {
                int64_t now_us = esp_timer_get_time();
                int64_t held_ms = (now_us - down_us) / 1000;
                bool is_tap = (move_px <= TAP_MOVE_MAX_PX) && (held_ms <= TAP_MAX_MS);
                if (is_tap) {
                    bool dbl = (last_tap_us > 0) && ((now_us - last_tap_us) / 1000 <= DBLCLICK_GAP_MS);
                    hid_left_clicks(1);
                    if (dbl) {
                        last_tap_us = 0;
                        ESP_LOGI(TAG, "double-tap (select)");
                    } else {
                        last_tap_us = now_us;
                        ESP_LOGI(TAG, "tap (click)");
                    }
                } else {
                    last_tap_us = 0;
                    ESP_LOGI(TAG, "touch up move=%lu px (cursor only)", (unsigned long)move_px);
                }
                down = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_display";
static uint8_t s_dot[16 * 16 * 3];

static void fill_rgb888(uint8_t *buf, size_t pixels, uint8_t r, uint8_t g, uint8_t b)
{
    for (size_t i = 0; i < pixels; i++) {
        buf[i * 3 + 0] = b;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = r;
    }
}

void app_main(void)
{
    const bsp_display_config_t disp_cfg = { .num_fbs = 1 };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(bsp_display_new(&disp_cfg, &panel, NULL));
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    void *fb = NULL;
    ESP_ERROR_CHECK(bsp_display_get_framebuffers(&fb, NULL));
    fill_rgb888((uint8_t *)fb, (size_t)BSP_LCD_H_RES * BSP_LCD_V_RES, 16, 24, 40);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, fb));

    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp));
    ESP_LOGI(TAG, "touch ready, brightness sweep then draw dots");

    for (int p = 20; p <= 100; p += 20) {
        ESP_ERROR_CHECK(bsp_display_brightness_set(p));
        ESP_LOGI(TAG, "backlight %d%%", p);
        vTaskDelay(pdMS_TO_TICKS(400));
    }

    fill_rgb888(s_dot, 16 * 16, 0, 220, 220);

    while (1) {
        if (esp_lcd_touch_read_data(tp) == ESP_OK) {
            esp_lcd_touch_point_data_t pts[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
            uint8_t n = 0;
            if (esp_lcd_touch_get_data(tp, pts, &n, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) == ESP_OK && n > 0) {
                int x0 = pts[0].x > 8 ? pts[0].x - 8 : 0;
                int y0 = pts[0].y > 8 ? pts[0].y - 8 : 0;
                int x1 = x0 + 16;
                int y1 = y0 + 16;
                if (x1 > BSP_LCD_H_RES) x1 = BSP_LCD_H_RES;
                if (y1 > BSP_LCD_V_RES) y1 = BSP_LCD_V_RES;
                esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, s_dot);
                ESP_LOGI(TAG, "touch %d,%d", pts[0].x, pts[0].y);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "bsp/esp-bsp.h"

#define EXAMPLE_LCD_H_RES   BSP_LCD_H_RES
#define EXAMPLE_LCD_V_RES   BSP_LCD_V_RES
#define EXAMPLE_LCD_FB_SIZE BSP_LCD_FB_SIZE
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE  (1)
#define EXAMPLE_LCD_RGB_BUFFER_NUMS         BSP_LCD_FB_COUNT

static const char *LCD_TAG = "LCD";
static esp_lcd_panel_handle_t lcd_panel;
static void *lcd_fbs[BSP_LCD_FB_COUNT];
static SemaphoreHandle_t s_lcd_fb_ready;
static bool s_backlight_on;

IRAM_ATTR static bool lcd_on_fb_complete(esp_lcd_panel_handle_t panel,
                                         const esp_lcd_rgb_panel_event_data_t *edata,
                                         void *user_ctx)
{
    BaseType_t hp_task_awoken = pdFALSE;
    (void)panel;
    (void)edata;
    (void)user_ctx;
    if (s_lcd_fb_ready) {
        xSemaphoreGiveFromISR(s_lcd_fb_ready, &hp_task_awoken);
    }
    return hp_task_awoken == pdTRUE;
}

static void lcd_wait_fb_ready(void)
{
    if (s_lcd_fb_ready == NULL) {
        return;
    }
    xSemaphoreTake(s_lcd_fb_ready, 0);
    xSemaphoreTake(s_lcd_fb_ready, pdMS_TO_TICKS(50));
}

static void lcd_swap_framebuffer(const void *fb)
{
    /* 等当前扫描结束再切，避免第一帧画到正在扫的缓冲上 */
    lcd_wait_fb_ready();
    if (!s_backlight_on && lcd_fbs[0] && lcd_fbs[1] && fb) {
        if (fb != lcd_fbs[0]) {
            memcpy(lcd_fbs[0], fb, EXAMPLE_LCD_FB_SIZE);
        }
        if (fb != lcd_fbs[1]) {
            memcpy(lcd_fbs[1], fb, EXAMPLE_LCD_FB_SIZE);
        }
    }
    esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, fb);
    lcd_wait_fb_ready();
    if (!s_backlight_on) {
        bsp_display_backlight_on();
        s_backlight_on = true;
        ESP_LOGI(LCD_TAG, "backlight on after first frame");
    }
}

static esp_err_t app_lcd_init(void)
{
    const bsp_display_config_t disp_cfg = {
        .num_fbs = BSP_LCD_FB_COUNT,
        .bounce_lines = BSP_LCD_BOUNCE_BUFFER_HEIGHT,
    };
    ESP_ERROR_CHECK(bsp_display_new(&disp_cfg, &lcd_panel, NULL));
    /* 先保持背光关：PSRAM 缓冲清黑后再亮，避免开场闪一下 */
    ESP_ERROR_CHECK(bsp_display_backlight_off());
    ESP_ERROR_CHECK(bsp_display_get_framebuffers(&lcd_fbs[0], &lcd_fbs[1]));
    if (lcd_fbs[0]) {
        memset(lcd_fbs[0], 0, EXAMPLE_LCD_FB_SIZE);
    }
    if (lcd_fbs[1]) {
        memset(lcd_fbs[1], 0, EXAMPLE_LCD_FB_SIZE);
    }
    s_lcd_fb_ready = xSemaphoreCreateBinary();
    if (!s_lcd_fb_ready) {
        ESP_LOGE(LCD_TAG, "fb ready sem failed");
        return ESP_ERR_NO_MEM;
    }
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = lcd_on_fb_complete,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(lcd_panel, &cbs, NULL));
    ESP_LOGI(LCD_TAG, "BSP RGB LCD ready");
    return ESP_OK;
}

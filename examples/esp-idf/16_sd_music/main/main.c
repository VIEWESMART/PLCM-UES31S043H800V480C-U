#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "music_app.h"

static const char *TAG = "bsp_mp3";

void app_main(void)
{
    /* LVGL first (esp_lvgl_port, 40-line PSRAM buf) so I2S DMA does not
     * starve the RGB bounce ISR. */
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    ESP_ERROR_CHECK(music_app_init());

    if (bsp_display_lock(0)) {
        music_ui_start();
        bsp_display_unlock();
    }
    ESP_LOGI(TAG, "music player UI started");
}

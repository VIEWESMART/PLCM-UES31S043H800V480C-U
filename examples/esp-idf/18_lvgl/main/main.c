#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lv_demos.h"

static const char *TAG = "bsp_lvgl";

void app_main(void)
{
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    if (bsp_display_lock(0)) {
        lv_demo_widgets();
        bsp_display_unlock();
    }
    ESP_LOGI(TAG, "LVGL widgets demo via BSP + esp_lvgl_port");
}

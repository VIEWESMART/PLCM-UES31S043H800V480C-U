#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_led";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_led_rgb_init());
    ESP_ERROR_CHECK(bsp_buzzer_init());
    ESP_LOGI(TAG, "WS2812 GPIO37 + buzzer GPIO46. Watch the small RGB LED (not LCD), listen for beep");
    const uint8_t c[][3] = {{48, 0, 0}, {0, 48, 0}, {0, 0, 48}, {48, 48, 48}};
    const char *name[] = {"red", "green", "blue", "white"};
    int i = 0;
    while (1) {
        ESP_LOGI(TAG, "LED %s + beep", name[i]);
        ESP_ERROR_CHECK(bsp_led_rgb_set(c[i][0], c[i][1], c[i][2]));
        ESP_ERROR_CHECK(bsp_buzzer_beep(120, 0));
        i = (i + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

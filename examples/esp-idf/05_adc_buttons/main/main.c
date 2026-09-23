#include <stdint.h>
#include <stdlib.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_btn";
static const char *NAMES[] = {"SW3", "SW4", "SW5", "SW6"};

static void cb(void *h, void *arg)
{
    int idx = (int)(intptr_t)arg;
    ESP_LOGI(TAG, "%s event=%s raw=%d adc=%d mV", NAMES[idx],
             iot_button_get_event_str(iot_button_get_event(h)),
             bsp_adc_read_raw(), bsp_adc_read_mv());
}

void app_main(void)
{
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        iot_button_register_cb(btns[i], BUTTON_SINGLE_CLICK, NULL, cb, (void *)(intptr_t)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, cb, (void *)(intptr_t)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_UP, NULL, cb, (void *)(intptr_t)i);
    }
    ESP_LOGI(TAG, "press KEY array SW3-SW6 (not BOOT / not touch), hold 1s each");
    int last_raw = -1;
    while (1) {
        int raw = bsp_adc_read_raw();
        int mv = bsp_adc_read_mv();
        if (last_raw < 0 || abs(raw - last_raw) > 8) {
            ESP_LOGI(TAG, "adc raw=%d mv=%d%s", raw, mv,
                     (raw <= 8) ? " (idle/N-sat)" : "");
            last_raw = raw;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

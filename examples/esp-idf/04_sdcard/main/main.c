#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_sd";

void app_main(void)
{
    esp_err_t err = ESP_FAIL;
    for (int n = 0; err != ESP_OK; n++) {
        err = bsp_sdcard_mount();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SD not ready (%s), insert card... retry %d",
                     esp_err_to_name(err), n + 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    DIR *d = opendir(BSP_SDCARD_MOUNT_POINT);
    if (!d) {
        ESP_LOGE(TAG, "opendir failed");
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        ESP_LOGI(TAG, "  %s", e->d_name);
    }
    closedir(d);
    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (card) {
        ESP_LOGI(TAG, "capacity %llu MB",
                 ((uint64_t)card->csd.capacity * card->csd.sector_size) >> 20);
    }
}

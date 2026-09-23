#include <inttypes.h>
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "driver/uart.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_rs485";

static void rx_task(void *arg)
{
    uint8_t buf[128];
    while (1) {
        int r = uart_read_bytes(UART_NUM_2, buf, sizeof(buf) - 1, pdMS_TO_TICKS(50));
        if (r > 0) {
            buf[r] = 0;
            ESP_LOGI(TAG, "rx %d: %s", r, (char *)buf);
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_rs485_init(115200));

    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    uint32_t period_ms = 1400 + (mac[5] % 9) * 220;
    ESP_LOGI(TAG, "mac %02x:%02x:%02x:%02x:%02x:%02x period %u ms",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (unsigned)period_ms);

    xTaskCreate(rx_task, "rs485_rx", 4096, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(200 + (mac[5] % 10) * 80));

    uint32_t n = 0;
    while (1) {
        char msg[48];
        int len = snprintf(msg, sizeof(msg), "S31 BSP RS485 tick %" PRIu32 "\r\n", ++n);
        ESP_ERROR_CHECK(bsp_rs485_write(msg, len));
        ESP_LOGI(TAG, "tx %d: %s", len, msg);
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

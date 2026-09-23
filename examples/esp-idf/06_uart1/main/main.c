#include <inttypes.h>
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_uart1";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_uart1_init(115200));
    ESP_LOGI(TAG, "external TX-RX short, pins TX=%d RX=%d", BSP_UART1_TX, BSP_UART1_RX);

    uint32_t n = 0;
    uint8_t buf[128];
    while (1) {
        char msg[48];
        int len = snprintf(msg, sizeof(msg), "S31 BSP UART1 tick %" PRIu32 "\r\n", ++n);
        uart_write_bytes(UART_NUM_1, msg, len);
        uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100));
        int r = uart_read_bytes(UART_NUM_1, buf, sizeof(buf) - 1, pdMS_TO_TICKS(200));
        if (r > 0) {
            buf[r] = 0;
            ESP_LOGI(TAG, "rx %d: %s", r, (char *)buf);
        } else {
            ESP_LOGW(TAG, "tick %" PRIu32 " no echo", n);
        }
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}

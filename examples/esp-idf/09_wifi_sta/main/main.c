#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_wifi";

#ifndef EXAMPLE_WIFI_SSID
#define EXAMPLE_WIFI_SSID       "viewe"
#define EXAMPLE_WIFI_PASSWORD   "yysj29432299"
#endif

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *e = data;
    ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&e->ip_info.ip));
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_wifi_init_sta(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASSWORD));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));
    while (1) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            ESP_LOGI(TAG, "SSID=%s RSSI=%d ch=%d", ap.ssid, ap.rssi, ap.primary);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

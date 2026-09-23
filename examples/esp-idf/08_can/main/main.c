#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "bsp_can";

/*
 * 1：自环回，无对端也能发（不要求 ACK）。单板看 rx self；两板还能看到 rx peer。
 * 0：标准 ACK，需要另一块板或 USB-CAN，否则 TX 会失败/bus-off。
 */
#define EXAMPLE_CAN_SELF_TEST   1
#define RX_POOL 8

typedef struct {
    twai_frame_t frame;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} can_rx_item_t;

static twai_node_handle_t s_node;
static uint32_t s_tx_id;
static uint32_t s_period_ms;
static can_rx_item_t s_rx_pool[RX_POOL];
static volatile int s_rx_w;
static volatile int s_rx_r;
static SemaphoreHandle_t s_rx_sem;
static SemaphoreHandle_t s_free_sem;

static bool IRAM_ATTR on_rx_done(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    (void)edata;
    (void)user_ctx;
    BaseType_t woken = pdFALSE;

    if (xSemaphoreTakeFromISR(s_free_sem, &woken) != pdTRUE) {
        ESP_EARLY_LOGW(TAG, "rx pool full, drop");
        return woken == pdTRUE;
    }

    can_rx_item_t *item = &s_rx_pool[s_rx_w];
    item->frame.buffer = item->data;
    item->frame.buffer_len = sizeof(item->data);
    if (twai_node_receive_from_isr(handle, &item->frame) == ESP_OK) {
        s_rx_w = (s_rx_w + 1) % RX_POOL;
        xSemaphoreGiveFromISR(s_rx_sem, &woken);
    } else {
        xSemaphoreGiveFromISR(s_free_sem, &woken);
    }
    return woken == pdTRUE;
}

static bool IRAM_ATTR on_error(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    ESP_EARLY_LOGW(TAG, "bus error: 0x%" PRIx32, edata->err_flags.val);
    return false;
}

static bool IRAM_ATTR on_state_change(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx)
{
    (void)handle;
    (void)user_ctx;
    static const char *names[] = {"error_active", "error_warning", "error_passive", "bus_off"};
    ESP_EARLY_LOGI(TAG, "state %s -> %s", names[edata->old_sta], names[edata->new_sta]);
    return false;
}

static void rx_task(void *arg)
{
    (void)arg;
    while (1) {
        if (xSemaphoreTake(s_rx_sem, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        can_rx_item_t *item = &s_rx_pool[s_rx_r];
        uint16_t len = twaifd_dlc2len(item->frame.header.dlc);
        if (len > TWAI_FRAME_MAX_LEN) {
            len = TWAI_FRAME_MAX_LEN;
        }
        uint32_t n = 0;
        if (len >= 4) {
            n = ((uint32_t)item->data[0] << 24) | ((uint32_t)item->data[1] << 16) |
                ((uint32_t)item->data[2] << 8) | item->data[3];
        }
        const char *kind = (item->frame.header.id == s_tx_id) ? "self" : "peer";
        ESP_LOGI(TAG, "rx %s id=0x%03lX n=%" PRIu32,
                 kind, (unsigned long)item->frame.header.id, n);
        s_rx_r = (s_rx_r + 1) % RX_POOL;
        xSemaphoreGive(s_free_sem);
    }
}

void app_main(void)
{
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    s_tx_id = (mac[5] < 0x50) ? 0x123 : 0x321;
    s_period_ms = 1400 + (mac[5] % 9) * 220;

    s_rx_sem = xSemaphoreCreateCounting(RX_POOL, 0);
    s_free_sem = xSemaphoreCreateCounting(RX_POOL, RX_POOL);
    assert(s_rx_sem && s_free_sem);

    ESP_ERROR_CHECK(bsp_can_init(500000, EXAMPLE_CAN_SELF_TEST, &s_node));
    ESP_ERROR_CHECK(twai_node_disable(s_node));
    twai_event_callbacks_t cbs = {
        .on_rx_done = on_rx_done,
        .on_error = on_error,
        .on_state_change = on_state_change,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(s_node, &cbs, NULL));
    ESP_ERROR_CHECK(twai_node_enable(s_node));

    ESP_LOGI(TAG, "mac %02x:%02x:%02x:%02x:%02x:%02x tx_id=0x%03lX period %u ms self_test=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             (unsigned long)s_tx_id, (unsigned)s_period_ms, EXAMPLE_CAN_SELF_TEST);
    ESP_LOGI(TAG, "single board: rx self; two boards: CAN_H/CAN_L/GND, also rx peer");

    xTaskCreate(rx_task, "can_rx", 4096, NULL, 10, NULL);
    vTaskDelay(pdMS_TO_TICKS(400 + (mac[5] % 10) * 80));

    uint32_t n = 0;
    uint8_t payload[4];
    while (1) {
        n++;
        payload[0] = (uint8_t)(n >> 24);
        payload[1] = (uint8_t)(n >> 16);
        payload[2] = (uint8_t)(n >> 8);
        payload[3] = (uint8_t)n;

        twai_frame_t frame = {
            .header.id = s_tx_id,
            .header.dlc = sizeof(payload),
            .buffer = payload,
            .buffer_len = sizeof(payload),
        };

        twai_node_status_t status;
        twai_node_get_info(s_node, &status, NULL);
        if (status.state == TWAI_ERROR_BUS_OFF) {
            ESP_LOGW(TAG, "bus-off, recover");
            twai_node_recover(s_node);
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        esp_err_t err = twai_node_transmit(s_node, &frame, pdMS_TO_TICKS(200));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "tx id=0x%03lX n=%" PRIu32, (unsigned long)s_tx_id, n);
        } else {
            ESP_LOGW(TAG, "tx failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(s_period_ms));
    }
}

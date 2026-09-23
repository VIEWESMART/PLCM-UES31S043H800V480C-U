#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_mic";
static int16_t s_buf[512];

#define MIC_RATE       16000
#define MIC_CH         2
#define MIC_BITS       16
#define MIC_SECONDS    5
#define MIC_WAV_PATH   "/sdcard/bsp_mic.wav"

#pragma pack(push, 1)
typedef struct {
    char     riff[4];
    uint32_t chunk_size;
    char     wave[4];
    char     fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data[4];
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

static void fill_wav_header(wav_header_t *h, uint32_t data_bytes)
{
    memcpy(h->riff, "RIFF", 4);
    h->chunk_size = 36 + data_bytes;
    memcpy(h->wave, "WAVE", 4);
    memcpy(h->fmt, "fmt ", 4);
    h->fmt_size = 16;
    h->audio_format = 1;
    h->num_channels = MIC_CH;
    h->sample_rate = MIC_RATE;
    h->bits_per_sample = MIC_BITS;
    h->block_align = (uint16_t)(MIC_CH * MIC_BITS / 8);
    h->byte_rate = MIC_RATE * h->block_align;
    memcpy(h->data, "data", 4);
    h->data_size = data_bytes;
}

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
    esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
    if (!mic) {
        ESP_LOGE(TAG, "mic init failed");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = MIC_BITS,
        .channel = MIC_CH,
        .sample_rate = MIC_RATE,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(mic, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(mic, 24.5f));

    const int frames = 256;
    const int loops = MIC_RATE * MIC_SECONDS / frames;
    const uint32_t data_bytes = (uint32_t)loops * sizeof(s_buf);

    FILE *f = fopen(MIC_WAV_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "open %s failed", MIC_WAV_PATH);
        return;
    }
    wav_header_t hdr;
    fill_wav_header(&hdr, data_bytes);
    fwrite(&hdr, 1, sizeof(hdr), f);

    ESP_LOGI(TAG, "recording %ds to %s, speak now", MIC_SECONDS, MIC_WAV_PATH);
    for (int i = 0; i < loops; i++) {
        if (esp_codec_dev_read(mic, s_buf, sizeof(s_buf)) == ESP_OK) {
            fwrite(s_buf, 1, sizeof(s_buf), f);
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "done, copy %s to PC and play", MIC_WAV_PATH);
}

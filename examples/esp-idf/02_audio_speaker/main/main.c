#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdint.h>

static const char *TAG = "bsp_spk";
static int16_t s_tone[480 * 2];

void app_main(void)
{
    esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
    if (!spk) {
        ESP_LOGE(TAG, "speaker init failed");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = 48000,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(spk, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(spk, 60));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));
    ESP_LOGI(TAG, "1 kHz tone on speakers");

    const int samples = 480;
    float phase = 0;
    while (1) {
        for (int i = 0; i < samples; i++) {
            int16_t s = (int16_t)(sinf(phase) * 8000);
            s_tone[i * 2] = s;
            s_tone[i * 2 + 1] = s;
            phase += 2.0f * 3.1415926f * 1000.0f / 48000.0f;
            if (phase > 6.28f) phase -= 6.28f;
        }
        esp_codec_dev_write(spk, s_tone, sizeof(s_tone));
    }
}

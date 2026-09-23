#include "bsp_gmf_bridge.h"
#include "bsp/esp-bsp.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "bsp_gmf";
static dev_audio_codec_handles_t s_dac;
static dev_audio_codec_handles_t s_adc;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;

esp_err_t bsp_gmf_hw_init(void)
{
    /* BSP default: 2 RGB FBs + bounce 20, then I2S duplex. */
    ESP_RETURN_ON_ERROR(bsp_display_new(NULL, &s_panel, NULL), TAG, "display");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(bsp_touch_new(NULL, &s_touch), TAG, "touch");

    s_dac.codec_dev = bsp_audio_codec_speaker_init();
    s_adc.codec_dev = bsp_audio_codec_microphone_init();
    if (!s_dac.codec_dev || !s_adc.codec_dev) {
        ESP_LOGE(TAG, "codec init failed");
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(bsp_audio_poweramp_enable(true), TAG, "PA");
    ESP_LOGI(TAG, "BSP audio + LCD + touch ready for BT audio");
    return ESP_OK;
}

dev_audio_codec_handles_t *bsp_gmf_dac(void)
{
    return &s_dac;
}

dev_audio_codec_handles_t *bsp_gmf_adc(void)
{
    return &s_adc;
}

i2s_chan_handle_t bsp_gmf_i2s_tx(void)
{
    i2s_chan_handle_t tx = NULL;
    bsp_i2s_get_handles(&tx, NULL);
    return tx;
}

esp_lcd_panel_handle_t bsp_gmf_panel(void)
{
    return s_panel;
}

esp_lcd_touch_handle_t bsp_gmf_touch(void)
{
    return s_touch;
}

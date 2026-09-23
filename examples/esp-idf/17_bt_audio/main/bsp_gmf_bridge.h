#pragma once

#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

typedef struct {
    esp_codec_dev_handle_t codec_dev;
} dev_audio_codec_handles_t;

esp_err_t bsp_gmf_hw_init(void);
dev_audio_codec_handles_t *bsp_gmf_dac(void);
dev_audio_codec_handles_t *bsp_gmf_adc(void);
i2s_chan_handle_t bsp_gmf_i2s_tx(void);
esp_lcd_panel_handle_t bsp_gmf_panel(void);
esp_lcd_touch_handle_t bsp_gmf_touch(void);

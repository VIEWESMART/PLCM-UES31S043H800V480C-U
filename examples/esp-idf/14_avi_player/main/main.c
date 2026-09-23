#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "avi_player.h"
#include "bsp/esp-bsp.h"
#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_avi";
#define SAMPLE_RATE         44100
#define VOICE_VOLUME        55
#define AVI_FILE_PATH       "/sdcard/output.avi"
#define PCM_GAIN_NUM        4
#define PCM_GAIN_DEN        5

static esp_lcd_panel_handle_t lcd_panel;
static void *lcd_fbs[2];
static esp_codec_dev_handle_t codec_handle;
static avi_player_handle_t avi_handle;
static jpeg_decoder_handle_t jpeg_dec;
static uint8_t *jpeg_input_buf;
static size_t jpeg_input_buf_size;
static int lcd_draw_fb;
static char s_avi_path[288];
static bool s_got_video;
static bool s_got_audio;

static esp_err_t app_jpeg_hw_init(void)
{
    jpeg_decode_engine_cfg_t dec_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 80,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&dec_eng_cfg, &jpeg_dec));
    jpeg_decode_memory_alloc_cfg_t tx_cfg = {.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER};
    jpeg_input_buf = jpeg_alloc_decoder_mem(BSP_LCD_H_RES * BSP_LCD_V_RES, &tx_cfg, &jpeg_input_buf_size);
    if (jpeg_input_buf == NULL) {
        ESP_LOGE(TAG, "JPEG input buffer alloc failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool has_avi_ext(const char *name)
{
    size_t n = strlen(name);
    return n >= 4 && strcasecmp(name + n - 4, ".avi") == 0;
}

static void list_sd_and_find_avi(char *out, size_t out_sz)
{
    out[0] = '\0';
    DIR *d = opendir(BSP_SDCARD_MOUNT_POINT);
    if (!d) {
        ESP_LOGE(TAG, "cannot open %s", BSP_SDCARD_MOUNT_POINT);
        return;
    }
    ESP_LOGI(TAG, "SD files:");
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        ESP_LOGI(TAG, "  %s", e->d_name);
        if (out[0] == '\0' && has_avi_ext(e->d_name)) {
            snprintf(out, out_sz, "%s/%s", BSP_SDCARD_MOUNT_POINT, e->d_name);
        }
    }
    closedir(d);
    struct stat st;
    if (stat(AVI_FILE_PATH, &st) == 0) {
        strlcpy(out, AVI_FILE_PATH, out_sz);
    }
}

static void video_callback(frame_data_t *data, void *user_ctx)
{
    (void)user_ctx;
    if (data->type != FRAME_TYPE_VIDEO || !lcd_fbs[0]) {
        return;
    }
    if (data->data_bytes > jpeg_input_buf_size) {
        ESP_LOGE(TAG, "JPEG frame too large: %u > %u",
                 (unsigned)data->data_bytes, (unsigned)jpeg_input_buf_size);
        return;
    }
    memcpy(jpeg_input_buf, data->data, data->data_bytes);
    lcd_draw_fb ^= 1;
    uint8_t *fb = (uint8_t *)lcd_fbs[lcd_draw_fb];
    jpeg_decode_cfg_t decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t out_size = 0;
    if (jpeg_decoder_process(jpeg_dec, &decode_cfg, jpeg_input_buf, data->data_bytes,
                             fb, BSP_LCD_FB_SIZE, &out_size) != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed");
        return;
    }
    esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, fb);
    if (!s_got_video) {
        s_got_video = true;
        ESP_LOGI(TAG, "video OK %lux%lu JPEG",
                 (unsigned long)data->video_info.width, (unsigned long)data->video_info.height);
    }
}

static void audio_set_clock(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    (void)arg;
    if (!codec_handle) {
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate ? rate : SAMPLE_RATE,
        .bits_per_sample = bits_cfg ? bits_cfg : 16,
        .channel = ch ? ch : 2,
        .channel_mask = 0x03,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    ESP_LOGI(TAG, "AVI audio %lu Hz %lu bit %lu ch",
             (unsigned long)fs.sample_rate, (unsigned long)fs.bits_per_sample, (unsigned long)fs.channel);
    esp_codec_dev_close(codec_handle);
    if (esp_codec_dev_open(codec_handle, &fs) == ESP_OK) {
        esp_codec_dev_set_out_vol(codec_handle, VOICE_VOLUME);
    }
}

static void audio_callback(frame_data_t *data, void *user_ctx)
{
    (void)user_ctx;
    if (data->type != FRAME_TYPE_AUDIO || !codec_handle) {
        return;
    }
    int16_t *samples = (int16_t *)data->data;
    size_t count = data->data_bytes / sizeof(int16_t);
    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(((int32_t)samples[i] * PCM_GAIN_NUM) / PCM_GAIN_DEN);
    }
    if (esp_codec_dev_write(codec_handle, data->data, data->data_bytes) == ESP_OK && !s_got_audio) {
        s_got_audio = true;
        ESP_LOGI(TAG, "audio OK, listen to speaker");
    }
}

static void play_end(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "AVI ended, loop %s", s_avi_path);
    s_got_video = false;
    s_got_audio = false;
    if (avi_player_play_from_file(avi_handle, s_avi_path) != ESP_OK) {
        ESP_LOGE(TAG, "loop play failed");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_new(NULL, &lcd_panel, NULL));
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_ERROR_CHECK(bsp_display_get_framebuffers(&lcd_fbs[0], &lcd_fbs[1]));

    codec_handle = bsp_audio_codec_speaker_init();
    if (!codec_handle) {
        ESP_LOGE(TAG, "speaker init failed");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0x03,
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(codec_handle, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(codec_handle, VOICE_VOLUME));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));
    ESP_ERROR_CHECK(app_jpeg_hw_init());

    for (;;) {
        if (bsp_sdcard_mount() != ESP_OK) {
            ESP_LOGW(TAG, "insert SD card (FAT32), need output.avi");
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }
        list_sd_and_find_avi(s_avi_path, sizeof(s_avi_path));
        if (s_avi_path[0] == '\0') {
            ESP_LOGW(TAG, "no .avi on SD. Copy output.avi to card root (MJPEG + PCM, 800x480)");
            bsp_sdcard_unmount();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        break;
    }

    avi_player_config_t config = {
        .video_cb = video_callback,
        .audio_cb = audio_callback,
        .audio_set_clock_cb = audio_set_clock,
        .avi_play_end_cb = play_end,
        .user_data = codec_handle,
        .priority = 5,
        .stack_size = 8192 * 3,
        .buffer_size = 256 * 1024,
        .coreID = 1,
    };
    ESP_ERROR_CHECK(avi_player_init(config, &avi_handle));
    ESP_LOGI(TAG, "play %s", s_avi_path);
    ESP_ERROR_CHECK(avi_player_play_from_file(avi_handle, s_avi_path));
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

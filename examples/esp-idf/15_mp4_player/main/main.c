#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/jpeg_decode.h"
#include "esp_codec_dev.h"
#include "bsp/esp-bsp.h"

#include "esp_extractor.h"
#include "esp_extractor_defaults.h"
#include "decoder/esp_audio_dec.h"
#include "decoder/impl/esp_aac_dec.h"

#include "app_lcd.h"

static const char *TAG = "bsp_mp4";

#define SAMPLE_RATE         44100
#define VOICE_VOLUME        50
#define MP4_FILE_PATH       "/sdcard/output.mp4"
#define MP4_FILE_PATH_HQ    "/sdcard/output_hq.mp4"

#define EXTRACTOR_POOL_SIZE     (384 * 1024)
#define AUDIO_QUEUE_LEN         (12)
#define VIDEO_QUEUE_LEN         (5)
#define AUDIO_PCM_BUF_SIZE      (16 * 1024)
#define AV_SYNC_WAIT_MS         (40)
#define AV_SYNC_DROP_MS         (160)
#define JPEG_OUT_PAD_W          ((EXAMPLE_LCD_H_RES + 16 + 15) & ~15)
#define JPEG_OUT_PAD_H          ((EXAMPLE_LCD_V_RES + 16 + 15) & ~15)
#define JPEG_OUT_MAX_SIZE       (JPEG_OUT_PAD_W * JPEG_OUT_PAD_H * 3)
#define LOOP_PLAYBACK           (1)

typedef struct {
    uint32_t pts;
    uint32_t size;
    uint8_t data[];
} media_pkt_t;

typedef struct {
    FILE *fp;
    esp_extractor_handle_t extractor;
    esp_extractor_config_t ext_cfg;
    esp_audio_dec_handle_t aac_dec;
    QueueHandle_t audio_q;
    QueueHandle_t video_q;
    TaskHandle_t audio_task;
    TaskHandle_t video_task;
    volatile bool audio_run;
    volatile bool video_run;
    volatile uint32_t audio_pts;
    bool audio_started;
    bool has_audio;
    bool has_video;
    bool codec_cfg_ok;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits;
    uint16_t fps;
    esp_extractor_format_t audio_fmt;
    esp_extractor_format_t video_fmt;
    uint8_t *pcm_buf;
    uint32_t pcm_buf_size;
    int lcd_fb_idx;
    SemaphoreHandle_t aac_mux;
} mp4_player_t;

static esp_codec_dev_handle_t codec_handle = NULL;
static jpeg_decoder_handle_t jpeg_dec = NULL;
static uint8_t *jpeg_input_buf = NULL;
static size_t jpeg_input_buf_size = 0;
static uint8_t *jpeg_out_buf = NULL;
static size_t jpeg_out_buf_size = 0;
static mp4_player_t s_player;
static char s_mp4_path[288];
static bool s_got_video;
static bool s_got_audio;

static bool has_mp4_ext(const char *name)
{
    size_t n = strlen(name);
    return n >= 4 && strcasecmp(name + n - 4, ".mp4") == 0;
}

static void list_sd_and_find_mp4(char *out, size_t out_sz)
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
        if (out[0] == '\0' && has_mp4_ext(e->d_name)) {
            snprintf(out, out_sz, "%s/%s", BSP_SDCARD_MOUNT_POINT, e->d_name);
        }
    }
    closedir(d);
    struct stat st;
    if (stat(MP4_FILE_PATH_HQ, &st) == 0) {
        strlcpy(out, MP4_FILE_PATH_HQ, out_sz);
    }
    if (stat(MP4_FILE_PATH, &st) == 0) {
        strlcpy(out, MP4_FILE_PATH, out_sz);
    }
}

static esp_err_t wait_sd_and_mp4(void)
{
    for (;;) {
        if (bsp_sdcard_mount() != ESP_OK) {
            ESP_LOGW(TAG, "insert SD card (FAT32), need output.mp4");
            vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }
        list_sd_and_find_mp4(s_mp4_path, sizeof(s_mp4_path));
        if (s_mp4_path[0] == '\0') {
            ESP_LOGW(TAG, "no .mp4 on SD. Copy output.mp4 to card root (MJPEG yuvj420p + AAC)");
            bsp_sdcard_unmount();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        return ESP_OK;
    }
}

static esp_err_t app_jpeg_hw_init(void)
{
    jpeg_decode_engine_cfg_t dec_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 200,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&dec_eng_cfg, &jpeg_dec));

    jpeg_decode_memory_alloc_cfg_t tx_cfg = {.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER};
    jpeg_input_buf = jpeg_alloc_decoder_mem(512 * 1024, &tx_cfg, &jpeg_input_buf_size);
    if (jpeg_input_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JPEG input buffer");
        return ESP_FAIL;
    }
    jpeg_decode_memory_alloc_cfg_t rx_cfg = {.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    jpeg_out_buf = jpeg_alloc_decoder_mem(JPEG_OUT_MAX_SIZE, &rx_cfg, &jpeg_out_buf_size);
    if (jpeg_out_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JPEG output buffer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Hardware JPEG Decoder initialized (in=%u out=%u)",
             (unsigned)jpeg_input_buf_size, (unsigned)jpeg_out_buf_size);
    return ESP_OK;
}

static esp_err_t es8389_codec_init(void)
{
    codec_handle = bsp_audio_codec_speaker_init();
    if (!codec_handle) {
        return ESP_FAIL;
    }
    esp_codec_dev_sample_info_t sample_cfg = {
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channel = 2,
        .channel_mask = 0x03,
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(codec_handle, &sample_cfg));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(codec_handle, VOICE_VOLUME));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));
    ESP_LOGI(TAG, "BSP ES8389 speaker ready");
    return ESP_OK;
}

static esp_err_t audio_reopen(uint32_t rate, uint8_t bits, uint8_t ch)
{
    if (!codec_handle) {
        return ESP_FAIL;
    }
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate ? rate : SAMPLE_RATE,
        .bits_per_sample = bits ? bits : I2S_DATA_BIT_WIDTH_16BIT,
        .channel = 2,
        .channel_mask = 0x03,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    (void)ch;
    esp_codec_dev_close(codec_handle);
    if (esp_codec_dev_open(codec_handle, &fs) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reopen codec %lu Hz %u bit", (unsigned long)fs.sample_rate, fs.bits_per_sample);
        return ESP_FAIL;
    }
    esp_codec_dev_set_out_vol(codec_handle, VOICE_VOLUME);
    ESP_LOGI(TAG, "Audio clock: %lu Hz, %u bit, stereo out", (unsigned long)fs.sample_rate, fs.bits_per_sample);
    return ESP_OK;
}

static int mp4_in_read(void *buffer, uint32_t size, void *ctx)
{
    FILE *fp = (FILE *)ctx;
    return (int)fread(buffer, 1, size, fp);
}

static int mp4_in_seek(uint32_t position, void *ctx)
{
    FILE *fp = (FILE *)ctx;
    return fseek(fp, (long)position, SEEK_SET);
}

static uint32_t mp4_in_size(void *ctx)
{
    FILE *fp = (FILE *)ctx;
    long cur = ftell(fp);
    if (cur < 0) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        return 0;
    }
    long end = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return end > 0 ? (uint32_t)end : 0;
}

static bool aac_has_adts(const uint8_t *data, uint32_t len)
{
    return data && len >= 2 && data[0] == 0xFF && (data[1] & 0xF6) == 0xF0;
}

static void media_queue_drain(QueueHandle_t q)
{
    media_pkt_t *pkt = NULL;
    while (q && xQueueReceive(q, &pkt, 0) == pdTRUE) {
        heap_caps_free(pkt);
    }
}

static esp_err_t pcm_write_stereo(mp4_player_t *p, const uint8_t *pcm, uint32_t bytes, uint8_t src_ch)
{
    if (!codec_handle || !pcm || bytes == 0) {
        return ESP_OK;
    }

    if (src_ch <= 1) {
        uint32_t samples = bytes / sizeof(int16_t);
        uint32_t need = samples * 2 * sizeof(int16_t);
        if (need > p->pcm_buf_size) {
            uint8_t *nb = heap_caps_aligned_alloc(64, need, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!nb) {
                nb = heap_caps_aligned_alloc(64, need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            }
            if (!nb) {
                return ESP_ERR_NO_MEM;
            }
            heap_caps_free(p->pcm_buf);
            p->pcm_buf = nb;
            p->pcm_buf_size = need;
        }
        const int16_t *in = (const int16_t *)pcm;
        int16_t *out = (int16_t *)p->pcm_buf;
        for (uint32_t i = 0; i < samples; i++) {
            out[i * 2] = in[i];
            out[i * 2 + 1] = in[i];
        }
        return esp_codec_dev_write(codec_handle, p->pcm_buf, need);
    }

    return esp_codec_dev_write(codec_handle, (void *)pcm, bytes);
}

static esp_err_t aac_dec_ensure_open(mp4_player_t *p, const uint8_t *frame, uint32_t frame_size)
{
    if (p->aac_dec) {
        return ESP_OK;
    }

    bool no_adts = !aac_has_adts(frame, frame_size);
    esp_aac_dec_cfg_t aac_cfg = {
        .sample_rate = (int32_t)(p->sample_rate ? p->sample_rate : SAMPLE_RATE),
        .channel = p->channels ? p->channels : 2,
        .bits_per_sample = p->bits ? p->bits : 16,
        .no_adts_header = no_adts,
        .aac_plus_enable = true,
    };
    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_AAC,
        .cfg = &aac_cfg,
        .cfg_sz = sizeof(aac_cfg),
    };

    esp_audio_err_t ret = esp_audio_dec_open(&dec_cfg, &p->aac_dec);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "AAC decoder open failed: %d (no_adts=%d)", ret, (int)no_adts);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "AAC decoder ready (no_adts=%d, %lu Hz, %u ch)",
             (int)no_adts, (unsigned long)aac_cfg.sample_rate, aac_cfg.channel);
    return ESP_OK;
}

static void play_pcm_or_aac(mp4_player_t *p, media_pkt_t *pkt)
{
    if (p->audio_fmt == ESP_EXTRACTOR_AUDIO_FORMAT_PCM) {
        if (!p->codec_cfg_ok) {
            audio_reopen(p->sample_rate, p->bits, p->channels);
            p->codec_cfg_ok = true;
        }
        pcm_write_stereo(p, pkt->data, pkt->size, p->channels);
        p->audio_pts = pkt->pts;
        p->audio_started = true;
        if (!s_got_audio) {
            s_got_audio = true;
            ESP_LOGI(TAG, "audio OK, listen to speaker");
        }
        return;
    }

    if (aac_dec_ensure_open(p, pkt->data, pkt->size) != ESP_OK) {
        return;
    }

    if (p->aac_mux) {
        xSemaphoreTake(p->aac_mux, portMAX_DELAY);
    }

    esp_audio_dec_in_raw_t raw = {
        .buffer = pkt->data,
        .len = pkt->size,
    };

    while (raw.len > 0) {
        esp_audio_dec_out_frame_t out_frame = {
            .buffer = p->pcm_buf,
            .len = p->pcm_buf_size,
        };
        esp_audio_err_t ret = esp_audio_dec_process(p->aac_dec, &raw, &out_frame);
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            uint32_t new_size = out_frame.needed_size ? out_frame.needed_size : (p->pcm_buf_size * 2);
            uint8_t *nb = heap_caps_aligned_alloc(64, new_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!nb) {
                nb = heap_caps_aligned_alloc(64, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            }
            if (!nb) {
                ESP_LOGE(TAG, "PCM buffer realloc failed");
                break;
            }
            heap_caps_free(p->pcm_buf);
            p->pcm_buf = nb;
            p->pcm_buf_size = new_size;
            continue;
        }
        if (ret != ESP_AUDIO_ERR_OK) {
            break;
        }
        if (raw.consumed == 0) {
            break;
        }

        if (out_frame.decoded_size > 0) {
            esp_audio_dec_info_t info = {0};
            if (!p->codec_cfg_ok && esp_audio_dec_get_info(p->aac_dec, &info) == ESP_AUDIO_ERR_OK) {
                if (info.sample_rate) {
                    p->sample_rate = info.sample_rate;
                }
                if (info.channel) {
                    p->channels = info.channel;
                }
                if (info.bits_per_sample) {
                    p->bits = info.bits_per_sample;
                }
                audio_reopen(p->sample_rate, p->bits, p->channels);
                p->codec_cfg_ok = true;
            }
            pcm_write_stereo(p, out_frame.buffer, out_frame.decoded_size, p->channels ? p->channels : 2);
            p->audio_pts = pkt->pts;
            p->audio_started = true;
            if (!s_got_audio) {
                s_got_audio = true;
                ESP_LOGI(TAG, "audio OK, listen to speaker");
            }
        }

        raw.buffer += raw.consumed;
        raw.len -= raw.consumed;
    }
    if (p->aac_mux) {
        xSemaphoreGive(p->aac_mux);
    }
}

static void audio_task(void *arg)
{
    mp4_player_t *p = (mp4_player_t *)arg;
    media_pkt_t *pkt = NULL;

    while (p->audio_run) {
        if (xQueueReceive(p->audio_q, &pkt, pdMS_TO_TICKS(50)) != pdTRUE) {
            continue;
        }
        play_pcm_or_aac(p, pkt);
        heap_caps_free(pkt);
        pkt = NULL;
    }

    media_queue_drain(p->audio_q);
    p->audio_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t audio_task_start(mp4_player_t *p)
{
    if (!p->has_audio) {
        return ESP_OK;
    }
    p->audio_q = xQueueCreate(AUDIO_QUEUE_LEN, sizeof(media_pkt_t *));
    if (!p->audio_q) {
        return ESP_ERR_NO_MEM;
    }
    p->pcm_buf_size = AUDIO_PCM_BUF_SIZE;
    p->pcm_buf = heap_caps_aligned_calloc(64, 1, p->pcm_buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p->pcm_buf) {
        p->pcm_buf = heap_caps_aligned_calloc(64, 1, p->pcm_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!p->pcm_buf) {
        return ESP_ERR_NO_MEM;
    }
    p->audio_run = true;
    if (xTaskCreatePinnedToCore(audio_task, "mp4_audio", 8 * 1024, p, 7, &p->audio_task, 0) != pdPASS) {
        p->audio_run = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void audio_task_stop(mp4_player_t *p)
{
    p->audio_run = false;
    while (p->audio_task) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (p->audio_q) {
        media_queue_drain(p->audio_q);
        vQueueDelete(p->audio_q);
        p->audio_q = NULL;
    }
}

static bool video_av_sync(mp4_player_t *p, uint32_t video_pts)
{
    if (!p->has_audio) {
        static int64_t last_us;
        int64_t now = esp_timer_get_time();
        int64_t frame_us = 1000000 / (p->fps ? p->fps : 24);
        if (last_us > 0) {
            int64_t wait_us = frame_us - (now - last_us);
            if (wait_us > 1000) {
                vTaskDelay(pdMS_TO_TICKS((uint32_t)(wait_us / 1000)));
            }
        }
        last_us = esp_timer_get_time();
        return true;
    }
    if (!p->audio_started) {
        /* 等音频先灌进 I2S DMA，避免开头爆音和画面抢跑 */
        int wait_ms = 0;
        while (p->video_run && !p->audio_started && wait_ms < 300) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
        if (!p->audio_started) {
            return true;
        }
    }

    uint32_t audio_pts = p->audio_pts;
    /* 开场几帧不丢，避免黑屏后突然跳帧造成闪一下 */
    if (s_got_video && video_pts + AV_SYNC_DROP_MS < audio_pts) {
        return false;
    }
    while (p->video_run && video_pts > audio_pts + AV_SYNC_WAIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(5));
        audio_pts = p->audio_pts;
    }
    return true;
}

static void blit_jpeg_rgb888(uint8_t *dst, const uint8_t *src, uint32_t src_size)
{
    const uint32_t vis_w = EXAMPLE_LCD_H_RES;
    const uint32_t vis_h = EXAMPLE_LCD_V_RES;
    const uint32_t vis_stride = vis_w * 3;
    if (src_size <= EXAMPLE_LCD_FB_SIZE) {
        memcpy(dst, src, src_size);
        if (src_size < EXAMPLE_LCD_FB_SIZE) {
            memset(dst + src_size, 0, EXAMPLE_LCD_FB_SIZE - src_size);
        }
        return;
    }
    uint32_t stride = vis_stride;
    if ((src_size % vis_h) == 0) {
        stride = src_size / vis_h;
    } else {
        uint32_t pad_h = (vis_h + 15) & ~15u;
        if (pad_h && (src_size % pad_h) == 0) {
            stride = src_size / pad_h;
        }
    }
    for (uint32_t y = 0; y < vis_h; y++) {
        memcpy(dst + y * vis_stride, src + y * stride, vis_stride);
    }
}

static void render_jpeg_frame(mp4_player_t *p, const uint8_t *jpeg, uint32_t jpeg_size)
{
    if (!lcd_fbs[0] || !jpeg_dec || !jpeg_out_buf || jpeg_size == 0) {
        return;
    }
    if (jpeg_size > jpeg_input_buf_size) {
        ESP_LOGE(TAG, "JPEG frame too large: %" PRIu32 " > %u", jpeg_size, (unsigned)jpeg_input_buf_size);
        return;
    }

    memcpy(jpeg_input_buf, jpeg, jpeg_size);

    if (!s_got_video) {
        jpeg_decode_picture_info_t info = {0};
        if (jpeg_decoder_get_info(jpeg_input_buf, jpeg_size, &info) == ESP_OK) {
            ESP_LOGI(TAG, "JPEG %lux%lu sample=0x%08" PRIx32 " (420=ok, 422=re-encode yuvj420p)",
                     (unsigned long)info.width, (unsigned long)info.height, (uint32_t)info.sample_method);
        }
    }

    p->lcd_fb_idx ^= 1;
    uint8_t *fb = (uint8_t *)lcd_fbs[p->lcd_fb_idx];

    jpeg_decode_cfg_t decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t out_size = 0;
    esp_err_t ret = jpeg_decoder_process(jpeg_dec, &decode_cfg,
                                         jpeg_input_buf, jpeg_size,
                                         jpeg_out_buf, jpeg_out_buf_size,
                                         &out_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        return;
    }
    blit_jpeg_rgb888(fb, jpeg_out_buf, out_size);

    lcd_swap_framebuffer(fb);
    if (!s_got_video) {
        s_got_video = true;
        ESP_LOGI(TAG, "video OK JPEG frame out=%" PRIu32, out_size);
    }
}

static esp_err_t enqueue_media(QueueHandle_t q, const uint8_t *data, uint32_t size, uint32_t pts, TickType_t wait)
{
    if (!q || !data || size == 0) {
        return ESP_OK;
    }
    media_pkt_t *pkt = heap_caps_malloc(sizeof(media_pkt_t) + size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pkt) {
        pkt = heap_caps_malloc(sizeof(media_pkt_t) + size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!pkt) {
        return ESP_ERR_NO_MEM;
    }
    pkt->pts = pts;
    pkt->size = size;
    memcpy(pkt->data, data, size);
    if (xQueueSend(q, &pkt, wait) != pdTRUE) {
        heap_caps_free(pkt);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void video_task(void *arg)
{
    mp4_player_t *p = (mp4_player_t *)arg;
    media_pkt_t *pkt = NULL;

    while (p->video_run) {
        if (xQueueReceive(p->video_q, &pkt, pdMS_TO_TICKS(50)) != pdTRUE) {
            continue;
        }
        if (video_av_sync(p, pkt->pts)) {
            render_jpeg_frame(p, pkt->data, pkt->size);
        }
        heap_caps_free(pkt);
        pkt = NULL;
    }

    media_queue_drain(p->video_q);
    p->video_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t video_task_start(mp4_player_t *p)
{
    if (!p->has_video) {
        return ESP_OK;
    }
    p->video_q = xQueueCreate(VIDEO_QUEUE_LEN, sizeof(media_pkt_t *));
    if (!p->video_q) {
        return ESP_ERR_NO_MEM;
    }
    p->video_run = true;
    if (xTaskCreatePinnedToCore(video_task, "mp4_video", 8 * 1024, p, 5, &p->video_task, 1) != pdPASS) {
        p->video_run = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void video_task_stop(mp4_player_t *p)
{
    p->video_run = false;
    while (p->video_task) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (p->video_q) {
        media_queue_drain(p->video_q);
        vQueueDelete(p->video_q);
        p->video_q = NULL;
    }
}

static esp_err_t parse_streams(mp4_player_t *p)
{
    uint16_t audio_num = 0;
    uint16_t video_num = 0;
    esp_extractor_stream_info_t info = {0};

    if (esp_extractor_get_stream_num(p->extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, &audio_num) == ESP_EXTRACTOR_ERR_OK &&
            audio_num > 0) {
        if (esp_extractor_get_stream_info(p->extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0, &info) == ESP_EXTRACTOR_ERR_OK) {
            p->audio_fmt = info.audio_info.format;
            p->sample_rate = info.audio_info.sample_rate ? info.audio_info.sample_rate : SAMPLE_RATE;
            p->channels = info.audio_info.channel ? info.audio_info.channel : 2;
            p->bits = info.audio_info.bits_per_sample ? info.audio_info.bits_per_sample : 16;
            p->has_audio = (p->audio_fmt == ESP_EXTRACTOR_AUDIO_FORMAT_AAC ||
                            p->audio_fmt == ESP_EXTRACTOR_AUDIO_FORMAT_PCM);
            ESP_LOGI(TAG, "Audio fmt=0x%08" PRIx32 " %lu Hz %u ch %u bit dur=%lu",
                     (uint32_t)p->audio_fmt, (unsigned long)p->sample_rate, p->channels, p->bits,
                     (unsigned long)info.duration);
            if (!p->has_audio) {
                ESP_LOGW(TAG, "Unsupported audio codec, play video only");
            }
        }
    }

    if (esp_extractor_get_stream_num(p->extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO, &video_num) == ESP_EXTRACTOR_ERR_OK &&
            video_num > 0) {
        if (esp_extractor_get_stream_info(p->extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO, 0, &info) == ESP_EXTRACTOR_ERR_OK) {
            p->video_fmt = info.video_info.format;
            p->fps = info.video_info.fps ? info.video_info.fps : 24;
            p->has_video = (p->video_fmt == ESP_EXTRACTOR_VIDEO_FORMAT_MJPEG);
            ESP_LOGI(TAG, "Video fmt=0x%08" PRIx32 " %ux%u @ %u fps dur=%lu",
                     (uint32_t)p->video_fmt, info.video_info.width, info.video_info.height, p->fps,
                     (unsigned long)info.duration);
            if (!p->has_video) {
                ESP_LOGE(TAG, "Only MJPEG video is supported. Convert with:");
                ESP_LOGE(TAG, "ffmpeg -i in.mp4 -vf fps=24,scale=800:480 -c:v mjpeg -q:v 2 -pix_fmt yuvj420p -c:a aac -ar 44100 -ac 2 out.mp4");
                return ESP_ERR_NOT_SUPPORTED;
            }
        }
    }

    if (!p->has_audio && !p->has_video) {
        ESP_LOGE(TAG, "No playable audio/video stream");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static void mp4_play_loop(mp4_player_t *p)
{
    while (1) {
        esp_extractor_frame_info_t frame = {0};
        esp_extractor_err_t ret = esp_extractor_read_frame(p->extractor, &frame);
        if (ret == ESP_EXTRACTOR_ERR_WAITING_OUTPUT) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (ret == ESP_EXTRACTOR_ERR_SKIPPED) {
            continue;
        }
        if (ret == ESP_EXTRACTOR_ERR_EOS || EXTRACTOR_IS_EOS(frame.frame_flag)) {
            if (frame.frame_buffer) {
                esp_extractor_release_frame(p->extractor, &frame);
            }
#if LOOP_PLAYBACK
            ESP_LOGI(TAG, "Loop playback");
            s_got_video = false;
            s_got_audio = false;
            media_queue_drain(p->audio_q);
            media_queue_drain(p->video_q);
            p->audio_started = false;
            p->audio_pts = 0;
            if (p->aac_dec && p->aac_mux) {
                xSemaphoreTake(p->aac_mux, portMAX_DELAY);
                esp_audio_dec_reset(p->aac_dec);
                xSemaphoreGive(p->aac_mux);
            }
            if (esp_extractor_seek(p->extractor, 0) != ESP_EXTRACTOR_ERR_OK) {
                ESP_LOGW(TAG, "Seek to start failed, stop");
                break;
            }
            continue;
#else
            ESP_LOGI(TAG, "Playback finished");
            break;
#endif
        }
        if (ret != ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGE(TAG, "read_frame failed: %d", ret);
            if (frame.frame_buffer) {
                esp_extractor_release_frame(p->extractor, &frame);
            }
            break;
        }

        if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO && p->has_audio) {
            enqueue_media(p->audio_q, frame.frame_buffer, frame.frame_size, frame.pts, pdMS_TO_TICKS(200));
        } else if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO && p->has_video) {
            enqueue_media(p->video_q, frame.frame_buffer, frame.frame_size, frame.pts, pdMS_TO_TICKS(30));
        }

        if (frame.frame_buffer) {
            esp_extractor_release_frame(p->extractor, &frame);
        }
    }
}

static esp_err_t app_play_mp4(const char *path)
{
    memset(&s_player, 0, sizeof(s_player));
    mp4_player_t *p = &s_player;

    p->fp = fopen(path, "rb");
    if (!p->fp) {
        ESP_LOGE(TAG, "Open %s failed", path);
        return ESP_ERR_NOT_FOUND;
    }

    p->ext_cfg = (esp_extractor_config_t) {
        .type = ESP_EXTRACTOR_TYPE_MP4,
        .extract_mask = ESP_EXTRACT_MASK_AV,
        .in_read_cb = mp4_in_read,
        .in_seek_cb = mp4_in_seek,
        .in_size_cb = mp4_in_size,
        .in_ctx = p->fp,
        .out_pool_size = EXTRACTOR_POOL_SIZE,
        .out_align = 64,
    };

    if (esp_extractor_open(&p->ext_cfg, &p->extractor) != ESP_EXTRACTOR_ERR_OK) {
        ESP_LOGE(TAG, "extractor open failed");
        fclose(p->fp);
        p->fp = NULL;
        return ESP_FAIL;
    }
    if (esp_extractor_parse_stream(p->extractor) != ESP_EXTRACTOR_ERR_OK) {
        ESP_LOGE(TAG, "parse stream failed");
        goto fail;
    }
    if (parse_streams(p) != ESP_OK) {
        goto fail;
    }
    if (p->has_audio && p->audio_fmt == ESP_EXTRACTOR_AUDIO_FORMAT_AAC) {
        p->aac_mux = xSemaphoreCreateMutex();
        if (esp_aac_dec_register() != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "AAC decoder register failed");
            goto fail;
        }
        if (audio_task_start(p) != ESP_OK) {
            ESP_LOGE(TAG, "audio task start failed");
            goto fail;
        }
    } else if (p->has_audio) {
        if (audio_task_start(p) != ESP_OK) {
            ESP_LOGE(TAG, "audio task start failed");
            goto fail;
        }
    }
    if (video_task_start(p) != ESP_OK) {
        ESP_LOGE(TAG, "video task start failed");
        goto fail;
    }

    ESP_LOGI(TAG, "Playing %s", path);
    mp4_play_loop(p);

fail:
    video_task_stop(p);
    audio_task_stop(p);
    if (p->aac_dec) {
        esp_audio_dec_close(p->aac_dec);
        p->aac_dec = NULL;
    }
    if (p->pcm_buf) {
        heap_caps_free(p->pcm_buf);
        p->pcm_buf = NULL;
    }
    if (p->aac_mux) {
        vSemaphoreDelete(p->aac_mux);
        p->aac_mux = NULL;
    }
    if (p->extractor) {
        esp_extractor_close(p->extractor);
        p->extractor = NULL;
    }
    if (p->fp) {
        fclose(p->fp);
        p->fp = NULL;
    }
    return ESP_OK;
}

static void mp4_extract_task(void *arg)
{
    app_play_mp4((const char *)arg);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_lcd_init());
    ESP_ERROR_CHECK(es8389_codec_init());
    ESP_ERROR_CHECK(wait_sd_and_mp4());
    ESP_ERROR_CHECK(app_jpeg_hw_init());
    if (esp_extractor_register_default() != ESP_EXTRACTOR_ERR_OK) {
        ESP_LOGE(TAG, "extractor register failed");
        return;
    }

    ESP_LOGI(TAG, "play %s (MJPEG+AAC). Watch LCD, listen to speaker", s_mp4_path);
    xTaskCreatePinnedToCore(mp4_extract_task, "mp4_extract", 12 * 1024,
                            (void *)s_mp4_path, 6, NULL, 1);
}

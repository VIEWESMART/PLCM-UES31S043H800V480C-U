#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "bsp/esp-bsp.h"
#include "esp_audio_simple_player.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "music_app.h"

static const char *TAG = "bsp_mp3";
#define SAMPLE_RATE   44100
#define DEFAULT_VOL   50

#define CMD_OPEN_DIR   1
#define CMD_PLAY_IDX   2
#define CMD_PREV       3
#define CMD_NEXT       4
#define CMD_TOGGLE     5
#define CMD_VOL        6
#define CMD_EOF        7

typedef struct {
    int cmd;
    int arg;
    char path[MUSIC_PATH_MAX];
} music_cmd_t;

static esp_codec_dev_handle_t s_codec;
static esp_asp_handle_t s_player;
static SemaphoreHandle_t s_mu;
static QueueHandle_t s_cmdq;
static music_state_t s_st;
static bool s_got_audio;

static bool has_mp3_ext(const char *name)
{
    size_t n = strlen(name);
    return n >= 4 && strcasecmp(name + n - 4, ".mp3") == 0;
}

static void path_join(char *out, size_t out_sz, const char *dir, const char *name)
{
    if (strcmp(dir, "/") == 0) {
        snprintf(out, out_sz, "/%s", name);
    } else {
        snprintf(out, out_sz, "%s/%s", dir, name);
    }
}

static void parent_dir(const char *path, char *out, size_t out_sz)
{
    strlcpy(out, path, out_sz);
    char *slash = strrchr(out, '/');
    if (!slash || slash == out) {
        strlcpy(out, BSP_SDCARD_MOUNT_POINT, out_sz);
        return;
    }
    *slash = '\0';
    if (out[0] == '\0' || strncmp(out, BSP_SDCARD_MOUNT_POINT, strlen(BSP_SDCARD_MOUNT_POINT)) != 0) {
        strlcpy(out, BSP_SDCARD_MOUNT_POINT, out_sz);
    }
}

static void path_to_uri(const char *path, char *uri, size_t uri_sz)
{
    /* GMF file IO does not percent-decode. Pass the real FAT path. */
    snprintf(uri, uri_sz, "file://%s", path);
}

static int cmp_entry(const void *a, const void *b)
{
    const music_entry_t *ea = a;
    const music_entry_t *eb = b;
    if (ea->kind != eb->kind) {
        return (int)ea->kind - (int)eb->kind;
    }
    return strcasecmp(ea->name, eb->name);
}

static void scan_dir_locked(const char *dir)
{
    DIR *d = opendir(dir);
    s_st.count = 0;
    strlcpy(s_st.dir, dir, sizeof(s_st.dir));
    if (!d) {
        ESP_LOGE(TAG, "opendir %s failed", dir);
        s_st.gen++;
        return;
    }

    if (strcmp(dir, BSP_SDCARD_MOUNT_POINT) != 0 && s_st.count < MUSIC_MAX_ENTRIES) {
        music_entry_t *e = &s_st.entries[s_st.count++];
        e->kind = MUSIC_ENT_DIR;
        strlcpy(e->name, "..", sizeof(e->name));
        parent_dir(dir, e->path, sizeof(e->path));
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && s_st.count < MUSIC_MAX_ENTRIES) {
        if (de->d_name[0] == '.') {
            continue;
        }
        if (strcmp(de->d_name, "System Volume Information") == 0) {
            continue;
        }
        char full[MUSIC_PATH_MAX];
        path_join(full, sizeof(full), dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            music_entry_t *e = &s_st.entries[s_st.count++];
            e->kind = MUSIC_ENT_DIR;
            strlcpy(e->name, de->d_name, sizeof(e->name));
            strlcpy(e->path, full, sizeof(e->path));
        } else if (S_ISREG(st.st_mode) && has_mp3_ext(de->d_name)) {
            music_entry_t *e = &s_st.entries[s_st.count++];
            e->kind = MUSIC_ENT_MP3;
            strlcpy(e->name, de->d_name, sizeof(e->name));
            strlcpy(e->path, full, sizeof(e->path));
        }
    }
    closedir(d);

    int start = 0;
    if (s_st.count > 0 && strcmp(s_st.entries[0].name, "..") == 0) {
        start = 1;
    }
    if (s_st.count > start) {
        qsort(&s_st.entries[start], (size_t)(s_st.count - start), sizeof(music_entry_t), cmp_entry);
    }
    s_st.gen++;
    ESP_LOGI(TAG, "scan %s count=%d", dir, s_st.count);
}

static int first_mp3_index_locked(void)
{
    for (int i = 0; i < s_st.count; i++) {
        if (s_st.entries[i].kind == MUSIC_ENT_MP3) {
            return i;
        }
    }
    return -1;
}

static int next_mp3_index_locked(int from, int dir)
{
    if (s_st.count <= 0) {
        return -1;
    }
    int i = from;
    for (int n = 0; n < s_st.count; n++) {
        i += dir;
        if (i < 0) {
            i = s_st.count - 1;
        } else if (i >= s_st.count) {
            i = 0;
        }
        if (s_st.entries[i].kind == MUSIC_ENT_MP3) {
            return i;
        }
    }
    return -1;
}

static void apply_volume_locked(int vol)
{
    if (vol < 0) {
        vol = 0;
    }
    if (vol > 100) {
        vol = 100;
    }
    s_st.volume = vol;
    if (s_codec) {
        esp_codec_dev_set_out_vol(s_codec, vol);
    }
}

static void play_file(const char *path, const char *name, int index)
{
    char uri[MUSIC_PATH_MAX + 16];
    path_to_uri(path, uri, sizeof(uri));
    if (!s_player) {
        return;
    }
    (void)esp_audio_simple_player_stop(s_player);
    vTaskDelay(pdMS_TO_TICKS(120));
    if (esp_audio_simple_player_run(s_player, uri, NULL) == ESP_OK) {
        xSemaphoreTake(s_mu, portMAX_DELAY);
        s_st.play_index = index;
        s_st.playing = true;
        s_st.paused = false;
        strlcpy(s_st.title, name, sizeof(s_st.title));
        xSemaphoreGive(s_mu);
        s_got_audio = false;
        ESP_LOGI(TAG, "play %s", uri);
    } else {
        ESP_LOGE(TAG, "play failed %s", uri);
    }
}

static bool copy_mp3_locked(int index, char *path, size_t path_sz, char *name, size_t name_sz)
{
    if (index < 0 || index >= s_st.count || s_st.entries[index].kind != MUSIC_ENT_MP3) {
        return false;
    }
    strlcpy(path, s_st.entries[index].path, path_sz);
    strlcpy(name, s_st.entries[index].name, name_sz);
    return true;
}

static int audio_output_cb(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_handle_t dev = (esp_codec_dev_handle_t)ctx;
    if (dev && data && data_size > 0) {
        if (esp_codec_dev_write(dev, data, data_size) == ESP_OK && !s_got_audio) {
            s_got_audio = true;
            ESP_LOGI(TAG, "audio OK, listen to speaker");
        }
    }
    return data_size;
}

static int player_event_cb(esp_asp_event_pkt_t *pkt, void *ctx)
{
    (void)ctx;
    if (!pkt) {
        return 0;
    }
    if (pkt->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO && pkt->payload) {
        esp_asp_music_info_t *info = (esp_asp_music_info_t *)pkt->payload;
        ESP_LOGI(TAG, "MP3 %d Hz %u ch %u bit", info->sample_rate, info->channels, info->bits);
    } else if (pkt->type == ESP_ASP_EVENT_TYPE_STATE && pkt->payload) {
        esp_asp_state_t st = *(esp_asp_state_t *)pkt->payload;
        ESP_LOGI(TAG, "player %s", esp_audio_simple_player_state_to_str(st));
        if (st == ESP_ASP_STATE_FINISHED) {
            music_cmd_t cmd = {.cmd = CMD_EOF};
            if (s_cmdq) {
                xQueueSend(s_cmdq, &cmd, 0);
            }
        } else if (st == ESP_ASP_STATE_ERROR) {
            xSemaphoreTake(s_mu, portMAX_DELAY);
            s_st.playing = false;
            xSemaphoreGive(s_mu);
            music_ui_notify();
        }
    }
    return 0;
}

static void player_task(void *arg)
{
    (void)arg;
    music_cmd_t cmd;
    while (xQueueReceive(s_cmdq, &cmd, portMAX_DELAY) == pdTRUE) {
        char path[MUSIC_PATH_MAX];
        char name[MUSIC_NAME_MAX];
        int play_idx = -1;
        bool do_play = false;
        bool do_pause = false;
        bool do_resume = false;

        xSemaphoreTake(s_mu, portMAX_DELAY);
        switch (cmd.cmd) {
        case CMD_OPEN_DIR: {
            char cur_path[MUSIC_PATH_MAX] = {0};
            if (s_st.play_index >= 0 && s_st.play_index < s_st.count) {
                strlcpy(cur_path, s_st.entries[s_st.play_index].path, sizeof(cur_path));
            }
            scan_dir_locked(cmd.path[0] ? cmd.path : BSP_SDCARD_MOUNT_POINT);
            s_st.play_index = -1;
            if (cur_path[0]) {
                for (int i = 0; i < s_st.count; i++) {
                    if (s_st.entries[i].kind == MUSIC_ENT_MP3 &&
                        strcmp(s_st.entries[i].path, cur_path) == 0) {
                        s_st.play_index = i;
                        break;
                    }
                }
            }
            break;
        }
        case CMD_PLAY_IDX:
            do_play = copy_mp3_locked(cmd.arg, path, sizeof(path), name, sizeof(name));
            play_idx = cmd.arg;
            break;
        case CMD_PREV:
            play_idx = next_mp3_index_locked(s_st.play_index, -1);
            do_play = copy_mp3_locked(play_idx, path, sizeof(path), name, sizeof(name));
            break;
        case CMD_NEXT:
        case CMD_EOF:
            play_idx = next_mp3_index_locked(s_st.play_index, 1);
            do_play = copy_mp3_locked(play_idx, path, sizeof(path), name, sizeof(name));
            break;
        case CMD_TOGGLE:
            if (s_st.playing && !s_st.paused && s_player) {
                do_pause = true;
            } else if (s_st.playing && s_st.paused && s_player) {
                do_resume = true;
            } else {
                play_idx = (s_st.play_index >= 0) ? s_st.play_index : first_mp3_index_locked();
                do_play = copy_mp3_locked(play_idx, path, sizeof(path), name, sizeof(name));
            }
            break;
        case CMD_VOL:
            apply_volume_locked(cmd.arg);
            break;
        default:
            break;
        }
        xSemaphoreGive(s_mu);

        if (do_play) {
            play_file(path, name, play_idx);
        } else if (do_pause) {
            if (esp_audio_simple_player_pause(s_player) == ESP_OK) {
                xSemaphoreTake(s_mu, portMAX_DELAY);
                s_st.paused = true;
                xSemaphoreGive(s_mu);
            }
        } else if (do_resume) {
            if (esp_audio_simple_player_resume(s_player) == ESP_OK) {
                xSemaphoreTake(s_mu, portMAX_DELAY);
                s_st.paused = false;
                xSemaphoreGive(s_mu);
            }
        }
        music_ui_notify();
    }
}

static void post_cmd(int id, int arg, const char *path)
{
    music_cmd_t cmd = {.cmd = id, .arg = arg, .path = {0}};
    if (path) {
        strlcpy(cmd.path, path, sizeof(cmd.path));
    }
    if (s_cmdq) {
        xQueueSend(s_cmdq, &cmd, pdMS_TO_TICKS(200));
    }
}

esp_err_t music_app_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.play_index = -1;
    s_st.volume = DEFAULT_VOL;
    strlcpy(s_st.dir, BSP_SDCARD_MOUNT_POINT, sizeof(s_st.dir));

    s_mu = xSemaphoreCreateMutex();
    s_cmdq = xQueueCreate(8, sizeof(music_cmd_t));
    if (!s_mu || !s_cmdq) {
        return ESP_ERR_NO_MEM;
    }

    s_codec = bsp_audio_codec_speaker_init();
    if (!s_codec) {
        ESP_LOGE(TAG, "speaker init failed");
        return ESP_FAIL;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0x03,
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(s_codec, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(s_codec, DEFAULT_VOL));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));

    for (;;) {
        if (bsp_sdcard_mount() == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "insert SD card (FAT32)");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    xSemaphoreTake(s_mu, portMAX_DELAY);
    scan_dir_locked(BSP_SDCARD_MOUNT_POINT);
    int first = first_mp3_index_locked();
    if (first < 0) {
        for (int i = 0; i < s_st.count; i++) {
            if (s_st.entries[i].kind == MUSIC_ENT_DIR &&
                strcasecmp(s_st.entries[i].name, "music") == 0) {
                scan_dir_locked(s_st.entries[i].path);
                first = first_mp3_index_locked();
                break;
            }
        }
    }
    xSemaphoreGive(s_mu);

    esp_asp_cfg_t cfg = {
        .out.cb = audio_output_cb,
        .out.user_ctx = s_codec,
        .task_prio = 5,
        .task_stack = 8192,
        .task_core = 1,
        .task_stack_in_ext = true,
    };
    ESP_ERROR_CHECK(esp_audio_simple_player_new(&cfg, &s_player));
    ESP_ERROR_CHECK(esp_audio_simple_player_set_event(s_player, player_event_cb, NULL));

    if (xTaskCreatePinnedToCore(player_task, "music_cmd", 8192, NULL, 6, NULL, 1) != pdPASS) {
        return ESP_FAIL;
    }

    if (first >= 0) {
        post_cmd(CMD_PLAY_IDX, first, NULL);
    }
    return ESP_OK;
}

void music_app_get_state(music_state_t *out)
{
    if (!out) {
        return;
    }
    xSemaphoreTake(s_mu, portMAX_DELAY);
    *out = s_st;
    xSemaphoreGive(s_mu);
}

void music_app_open_dir(const char *path)
{
    post_cmd(CMD_OPEN_DIR, 0, path);
}

void music_app_play_index(int index)
{
    post_cmd(CMD_PLAY_IDX, index, NULL);
}

void music_app_activate_index(int index)
{
    music_entry_t e;
    bool ok = false;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    if (index >= 0 && index < s_st.count) {
        e = s_st.entries[index];
        ok = true;
    }
    xSemaphoreGive(s_mu);
    if (!ok) {
        return;
    }
    if (e.kind == MUSIC_ENT_DIR) {
        post_cmd(CMD_OPEN_DIR, 0, e.path);
    } else {
        post_cmd(CMD_PLAY_IDX, index, NULL);
    }
}

void music_app_prev(void)
{
    post_cmd(CMD_PREV, 0, NULL);
}

void music_app_next(void)
{
    post_cmd(CMD_NEXT, 0, NULL);
}

void music_app_toggle(void)
{
    post_cmd(CMD_TOGGLE, 0, NULL);
}

void music_app_set_volume(int vol)
{
    post_cmd(CMD_VOL, vol, NULL);
}

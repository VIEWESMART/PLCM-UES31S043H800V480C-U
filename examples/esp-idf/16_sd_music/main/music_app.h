#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define MUSIC_MAX_ENTRIES  96
#define MUSIC_NAME_MAX     96
#define MUSIC_PATH_MAX     288

typedef enum {
    MUSIC_ENT_DIR = 0,
    MUSIC_ENT_MP3,
} music_ent_kind_t;

typedef struct {
    music_ent_kind_t kind;
    char name[MUSIC_NAME_MAX];
    char path[MUSIC_PATH_MAX];
} music_entry_t;

typedef struct {
    char dir[MUSIC_PATH_MAX];
    music_entry_t entries[MUSIC_MAX_ENTRIES];
    int count;
    int play_index;
    bool playing;
    bool paused;
    int volume;
    char title[MUSIC_NAME_MAX];
    unsigned gen;
} music_state_t;

esp_err_t music_app_init(void);
void music_app_get_state(music_state_t *out);
void music_app_open_dir(const char *path);
void music_app_play_index(int index);
void music_app_activate_index(int index);
void music_app_prev(void);
void music_app_next(void);
void music_app_toggle(void);
void music_app_set_volume(int vol);

void music_ui_start(void);
void music_ui_notify(void);

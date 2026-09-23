#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"
#include "music_app.h"

static const char *TAG = "music_ui";

LV_FONT_DECLARE(lv_font_notosanssc_regular_28)

#define UI_W        800
#define UI_H        480
#define BAR_H       56
#define VOL_W       56
#define LIST_W      340
#define COLOR_BG    0x102840
#define COLOR_PANEL 0x1a3a58
#define COLOR_BTN   0x2b5278
#define COLOR_ACC   0x42a5f5
#define COLOR_TEXT  0xe8eef4
#define COLOR_MUTE  0x8aa0b5
#define COLOR_SEL   0x2a5a86

#define FONT_TEXT   (&lv_font_notosanssc_regular_28)
#define FONT_ICON   (&lv_font_montserrat_28)

static lv_obj_t *s_path_lbl;
static lv_obj_t *s_list;
static lv_obj_t *s_title_lbl;
static lv_obj_t *s_info_lbl;
static lv_obj_t *s_play_lbl;
static lv_obj_t *s_vol_lbl;
static lv_obj_t *s_vol_slider;
static unsigned s_list_gen;
static int s_hl_index = -2;
static bool s_slider_dragging;
static music_state_t s_snap;
static char s_last_path[MUSIC_PATH_MAX];
static char s_last_title[MUSIC_NAME_MAX];
static char s_last_info[48];
static int s_last_vol = -1;
static int s_last_icon = -1;

static void style_icon_btn(lv_obj_t *btn, int w, int h)
{
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_BTN), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
}

static void on_list_item(lv_event_t *e)
{
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    music_app_activate_index(idx);
}

static void refresh_list(const music_state_t *st)
{
    lv_obj_clean(s_list);
    for (int i = 0; i < st->count; i++) {
        const music_entry_t *ent = &st->entries[i];
        const char *icon = (ent->kind == MUSIC_ENT_DIR) ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_AUDIO;
        lv_obj_t *btn = lv_list_add_button(s_list, icon, ent->name);
        lv_obj_set_style_text_color(btn, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex((ent->kind == MUSIC_ENT_MP3 && i == st->play_index) ?
                                                    COLOR_SEL : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        if (lv_obj_get_child_count(btn) > 0) {
            lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), FONT_ICON, 0);
        }
        if (lv_obj_get_child_count(btn) > 1) {
            lv_obj_t *lbl = lv_obj_get_child(btn, 1);
            lv_obj_set_style_text_font(lbl, FONT_TEXT, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_TEXT), 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        }
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, NULL);
    }
    s_list_gen = st->gen;
    s_hl_index = st->play_index;
}

static void highlight_playing(int play_index)
{
    uint32_t n = lv_obj_get_child_count(s_list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *btn = lv_obj_get_child(s_list, i);
        int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
        lv_obj_set_style_bg_color(btn, lv_color_hex((idx == play_index) ? COLOR_SEL : COLOR_PANEL), 0);
    }
    s_hl_index = play_index;
}

static void on_prev(lv_event_t *e)
{
    (void)e;
    music_app_prev();
}

static void on_next(lv_event_t *e)
{
    (void)e;
    music_app_next();
}

static void on_toggle(lv_event_t *e)
{
    (void)e;
    music_app_toggle();
}

static void on_slider(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    if (code == LV_EVENT_PRESSED) {
        s_slider_dragging = true;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_slider_dragging = false;
        music_app_set_volume((int)lv_slider_get_value(slider));
    } else if (code == LV_EVENT_VALUE_CHANGED && s_slider_dragging) {
        lv_label_set_text_fmt(s_vol_lbl, "%d", (int)lv_slider_get_value(slider));
    }
}

static void set_text_if_changed(lv_obj_t *lbl, char *cache, size_t cache_sz, const char *text)
{
    if (strcmp(cache, text) == 0) {
        return;
    }
    strlcpy(cache, text, cache_sz);
    lv_label_set_text(lbl, text);
}

static void ui_tick(lv_timer_t *t)
{
    (void)t;
    music_app_get_state(&s_snap);

    char path_txt[MUSIC_PATH_MAX + 8];
    snprintf(path_txt, sizeof(path_txt), "目录  %s", s_snap.dir);
    set_text_if_changed(s_path_lbl, s_last_path, sizeof(s_last_path), path_txt);
    set_text_if_changed(s_title_lbl, s_last_title, sizeof(s_last_title),
                        s_snap.title[0] ? s_snap.title : "选择一首歌曲");

    const char *info;
    const char *icon;
    int icon_id;
    if (s_snap.playing && s_snap.paused) {
        info = "已暂停";
        icon = LV_SYMBOL_PLAY;
        icon_id = 2;
    } else if (s_snap.playing) {
        info = "正在播放";
        icon = LV_SYMBOL_PAUSE;
        icon_id = 1;
    } else {
        info = s_snap.count ? "点左侧列表打开目录或歌曲" : "空目录";
        icon = LV_SYMBOL_PLAY;
        icon_id = 0;
    }
    set_text_if_changed(s_info_lbl, s_last_info, sizeof(s_last_info), info);
    if (icon_id != s_last_icon) {
        s_last_icon = icon_id;
        lv_label_set_text(s_play_lbl, icon);
    }
    if (!s_slider_dragging && s_snap.volume != s_last_vol) {
        s_last_vol = s_snap.volume;
        lv_slider_set_value(s_vol_slider, s_snap.volume, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_vol_lbl, "%d", s_snap.volume);
    }
    if (s_snap.gen != s_list_gen) {
        refresh_list(&s_snap);
    } else if (s_snap.play_index != s_hl_index) {
        highlight_playing(s_snap.play_index);
    }
}

static lv_obj_t *make_icon_btn(lv_obj_t *parent, const char *sym, lv_event_cb_t cb, int w, int h)
{
    lv_obj_t *btn = lv_button_create(parent);
    style_icon_btn(btn, w, h);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, sym);
    lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    if (cb == on_toggle) {
        s_play_lbl = lbl;
    }
    return btn;
}

void music_ui_start(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, UI_W, BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0c2034), 0);
    lv_obj_set_style_pad_hor(bar, 16, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "音乐播放器");
    lv_obj_set_style_text_font(title, FONT_TEXT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    s_path_lbl = lv_label_create(bar);
    lv_label_set_text(s_path_lbl, "目录");
    lv_label_set_long_mode(s_path_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_path_lbl, 460);
    lv_obj_set_style_text_font(s_path_lbl, FONT_TEXT, 0);
    lv_obj_set_style_text_color(s_path_lbl, lv_color_hex(COLOR_MUTE), 0);
    lv_obj_align(s_path_lbl, LV_ALIGN_RIGHT_MID, 0, 0);

    s_list = lv_list_create(scr);
    lv_obj_set_size(s_list, LIST_W, UI_H - BAR_H);
    lv_obj_set_pos(s_list, 0, BAR_H);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 8, 0);
    lv_obj_set_style_text_font(s_list, FONT_TEXT, 0);

    const int mid_w = UI_W - LIST_W - VOL_W;
    lv_obj_t *right = lv_obj_create(scr);
    lv_obj_set_size(right, mid_w, UI_H - BAR_H);
    lv_obj_set_pos(right, LIST_W, BAR_H);
    lv_obj_set_style_bg_color(right, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_radius(right, 0, 0);
    lv_obj_set_style_pad_all(right, 16, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cover = lv_obj_create(right);
    lv_obj_set_size(cover, 160, 160);
    lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_radius(cover, 16, 0);
    lv_obj_set_style_bg_color(cover, lv_color_hex(0x3d4f63), 0);
    lv_obj_set_style_border_width(cover, 0, 0);
    lv_obj_clear_flag(cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *note = lv_label_create(cover);
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(note, FONT_ICON, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0xa8b8c8), 0);
    lv_obj_center(note);

    s_title_lbl = lv_label_create(right);
    lv_label_set_text(s_title_lbl, "选择一首歌曲");
    lv_label_set_long_mode(s_title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_title_lbl, mid_w - 32);
    lv_obj_set_style_text_align(s_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_title_lbl, FONT_TEXT, 0);
    lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 180);

    s_info_lbl = lv_label_create(right);
    lv_label_set_text(s_info_lbl, "");
    lv_obj_set_style_text_font(s_info_lbl, FONT_TEXT, 0);
    lv_obj_set_style_text_color(s_info_lbl, lv_color_hex(COLOR_MUTE), 0);
    lv_obj_align(s_info_lbl, LV_ALIGN_TOP_MID, 0, 230);

    lv_obj_t *btns = lv_obj_create(right);
    lv_obj_set_size(btns, 360, 80);
    lv_obj_align(btns, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
    make_icon_btn(btns, LV_SYMBOL_PREV, on_prev, 88, 72);
    make_icon_btn(btns, LV_SYMBOL_PAUSE, on_toggle, 96, 72);
    make_icon_btn(btns, LV_SYMBOL_NEXT, on_next, 88, 72);

    lv_obj_t *vol_col = lv_obj_create(scr);
    lv_obj_set_size(vol_col, VOL_W, UI_H - BAR_H);
    lv_obj_set_pos(vol_col, UI_W - VOL_W, BAR_H);
    lv_obj_set_style_bg_color(vol_col, lv_color_hex(0x0c2034), 0);
    lv_obj_set_style_border_width(vol_col, 0, 0);
    lv_obj_set_style_radius(vol_col, 0, 0);
    lv_obj_set_style_pad_all(vol_col, 0, 0);
    lv_obj_clear_flag(vol_col, LV_OBJ_FLAG_SCROLLABLE);

    s_vol_lbl = lv_label_create(vol_col);
    lv_label_set_text(s_vol_lbl, "50");
    lv_obj_set_style_text_font(s_vol_lbl, FONT_TEXT, 0);
    lv_obj_set_style_text_color(s_vol_lbl, lv_color_hex(COLOR_MUTE), 0);
    lv_obj_align(s_vol_lbl, LV_ALIGN_TOP_MID, 0, 12);

    s_vol_slider = lv_slider_create(vol_col);
    lv_obj_set_size(s_vol_slider, 18, UI_H - BAR_H - 80);
    lv_slider_set_range(s_vol_slider, 0, 100);
    lv_slider_set_value(s_vol_slider, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(0x3d4f63), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(COLOR_ACC), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_vol_slider, 8, LV_PART_KNOB);
    lv_obj_align(s_vol_slider, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(s_vol_slider, on_slider, LV_EVENT_ALL, NULL);

    music_app_get_state(&s_snap);
    refresh_list(&s_snap);
    ui_tick(NULL);
    /* DOUBLE_DIRECT: paint both PSRAM FBs before audio starts. */
    lv_obj_invalidate(scr);
    lv_refr_now(NULL);
    lv_obj_invalidate(scr);
    lv_refr_now(NULL);
    ESP_LOGI(TAG, "LVGL music UI ready");
}

static void ui_async(void *p)
{
    (void)p;
    ui_tick(NULL);
}

void music_ui_notify(void)
{
    lv_async_call(ui_async, NULL);
}

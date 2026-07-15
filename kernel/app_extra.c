/******************************************************************************
 * torOS - Terminal Operating System
 * Application Extras - Dialogs, File Picker, App Utilities
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/app.h"
#include "../include/window.h"
#include "../include/widget.h"
#include "../include/font.h"
#include "../include/network.h"
#include "../include/image.h"

/* ===== Dialog Helpers ===== */

static dialog_t active_dialog;
static int dialog_active = 0;

void dialog_init(dialog_t *dlg, dialog_type_t type, const char *title, const char *message)
{
    if (!dlg) return;
    memset(dlg, 0, sizeof(dialog_t));
    dlg->type = type;
    strncpy(dlg->title, title ? title : "", DIALOG_TITLE_LEN - 1);
    strncpy(dlg->message, message ? message : "", DIALOG_MSG_LEN - 1);
    dlg->result = DIALOG_RESULT_NONE;
    dlg->modal = 1;
}

void dialog_show(dialog_t *dlg)
{
    if (!dlg) return;
    active_dialog = *dlg;
    dialog_active = 1;
    printk_color(TERM_CYAN, "[DIALOG] %s: %s\n", dlg->title, dlg->message);
}

dialog_result_t dialog_show_message(const char *title, const char *message, dialog_type_t type)
{
    dialog_t dlg;
    dialog_init(&dlg, type, title, message);
    dialog_show(&dlg);

    if (type == DIALOG_MESSAGE)
        return DIALOG_RESULT_OK;
    return DIALOG_RESULT_NONE;
}

void dialog_draw(dialog_t *dlg, uint32 *fb, int fb_w, int fb_h)
{
    if (!dlg || !fb) return;

    int dw = 400;
    int dh = 200;
    int dx = (fb_w - dw) / 2;
    int dy = (fb_h - dh) / 2;

    /* Shadow */
    for (int row = 4; row < dh + 4; row++)
        for (int col = 4; col < dw + 4; col++)
            if ((dy + row) < fb_h && (dx + col) < fb_w)
                fb[(dy + row) * fb_w + dx + col] = 0x80000000;

    /* Background */
    for (int row = 0; row < dh; row++)
        for (int col = 0; col < dw; col++)
            fb[(dy + row) * fb_w + dx + col] = 0xFFF0F0F0;

    /* Title bar */
    for (int row = 0; row < 32; row++)
        for (int col = 0; col < dw; col++)
            fb[(dy + row) * fb_w + dx + col] = 0xFF0078D7;

    fb_set_color(0xFFFFFFFF);
    fb_draw_string(dx + 8, dy + 6, dlg->title);

    /* Message */
    fb_set_color(0xFF000000);
    fb_draw_string(dx + 16, dy + 50, dlg->message);

    /* Buttons based on type */
    int btn_y = dy + dh - 50;
    if (dlg->type == DIALOG_MESSAGE) {
        for (int row = 0; row < 32; row++)
            for (int col = 0; col < 80; col++)
                fb[(btn_y + row) * fb_w + dx + dw / 2 - 40 + col] = 0xFFE1E1E1;
        fb_set_color(0xFF000000);
        fb_draw_string(dx + dw / 2 - 16, btn_y + 6, "OK");
    } else if (dlg->type == DIALOG_CONFIRM) {
        for (int row = 0; row < 32; row++)
            for (int col = 0; col < 80; col++) {
                fb[(btn_y + row) * fb_w + dx + dw / 2 - 90 + col] = 0xFFE1E1E1;
                fb[(btn_y + row) * fb_w + dx + dw / 2 + 10 + col] = 0xFFE1E1E1;
            }
        fb_set_color(0xFF000000);
        fb_draw_string(dx + dw / 2 - 66, btn_y + 6, "OK");
        fb_draw_string(dx + dw / 2 - 12, btn_y + 6, "Cancel");
    } else if (dlg->type == DIALOG_INPUT) {
        for (int row = 0; row < 28; row++)
            for (int col = 0; col < dw - 40; col++)
                fb[(dy + 90 + row) * fb_w + dx + 20 + col] = 0xFFFFFFFF;
        for (int row = 0; row < 32; row++)
            for (int col = 0; col < 80; col++)
                fb[(btn_y + row) * fb_w + dx + dw / 2 - 40 + col] = 0xFFE1E1E1;
        fb_set_color(0xFF000000);
        fb_draw_string(dx + dw / 2 - 16, btn_y + 6, "OK");
    }

    /* Border */
    for (int col = 0; col < dw; col++) {
        fb[dy * fb_w + dx + col] = 0xFF808080;
        fb[(dy + dh - 1) * fb_w + dx + col] = 0xFF808080;
    }
    for (int row = 0; row < dh; row++) {
        fb[(dy + row) * fb_w + dx] = 0xFF808080;
        fb[(dy + row) * fb_w + dx + dw - 1] = 0xFF808080;
    }
}

int dialog_is_active(void)
{
    return dialog_active;
}

void dialog_close(void)
{
    dialog_active = 0;
}

/* ===== File Picker ===== */

static file_picker_t active_picker;

void file_picker_init(file_picker_t *fp, file_picker_mode_t mode, const char *title)
{
    if (!fp) return;
    memset(fp, 0, sizeof(file_picker_t));
    fp->mode = mode;
    strncpy(fp->title, title ? title : (mode == FPICK_OPEN ? "Open" : "Save"), 63);
    strcpy(fp->current_path, "/");
    fp->filter_extensions[0][0] = '\0';
    fp->selected_file[0] = '\0';
}

void file_picker_set_filter(file_picker_t *fp, const char *ext)
{
    if (!fp || !ext) return;
    strncpy(fp->filter_extensions[0], ext, 15);
}

void file_picker_refresh(file_picker_t *fp)
{
    if (!fp) return;
    fp->file_count = 0;

    extern int tfs_count_used(void);
    extern int tfs_get_used_name(int idx, char *name_out, uint32 *size_out);

    int used = tfs_count_used();
    for (int i = 0; i < used && fp->file_count < FPICK_MAX_FILES; i++) {
        char name[64];
        uint32 size;
        if (tfs_get_used_name(i, name, &size) == 0) {
            if (fp->filter_extensions[0][0]) {
                char *dot = strrchr(name, '.');
                if (!dot || strcmp(dot + 1, fp->filter_extensions[0]) != 0)
                    continue;
            }
            strncpy(fp->files[fp->file_count].name, name, FPICK_MAX_FNAME - 1);
            fp->files[fp->file_count].size = size;
            fp->files[fp->file_count].is_dir = 0;
            fp->file_count++;
        }
    }
}

void file_picker_draw(file_picker_t *fp, uint32 *fb, int fb_w, int fb_h)
{
    if (!fp || !fb) return;

    int pw = 500;
    int ph = 400;
    int px = (fb_w - pw) / 2;
    int py = (fb_h - ph) / 2;

    /* Background */
    for (int row = 0; row < ph; row++)
        for (int col = 0; col < pw; col++)
            fb[(py + row) * fb_w + px + col] = 0xFFFFFFFF;

    /* Title bar */
    for (int row = 0; row < 30; row++)
        for (int col = 0; col < pw; col++)
            fb[(py + row) * fb_w + px + col] = 0xFF0078D7;

    fb_set_color(0xFFFFFFFF);
    fb_draw_string(px + 8, py + 6, fp->title);

    /* Path bar */
    for (int row = 0; row < 24; row++)
        for (int col = 0; col < pw - 16; col++)
            fb[(py + 38 + row) * fb_w + px + 8 + col] = 0xFFF0F0F0;
    fb_set_color(0xFF000000);
    fb_draw_string(px + 12, py + 42, fp->current_path);

    /* File list */
    int fy = py + 70;
    for (int i = 0; i < fp->file_count && fy < py + ph - 60; i++) {
        uint32 bg = (i == fp->selected_index) ? 0xFF0078D7 : 0xFFFFFFFF;
        uint32 fg = (i == fp->selected_index) ? 0xFFFFFFFF : 0xFF000000;

        for (int row = 0; row < 20 && (fy + row) < (py + ph - 60); row++)
            for (int col = 8; col < pw - 8; col++)
                fb[(fy + row) * fb_w + px + col] = bg;

        fb_set_color(fg);
        fb_draw_string(px + 16, fy + 2, fp->files[i].name);

        char sz[16];
        utoa(fp->files[i].size, sz, 10);
        fb_draw_string(px + pw - 80, fy + 2, sz);

        fy += 22;
    }

    /* Buttons */
    int btn_y = py + ph - 40;
    for (int row = 0; row < 28; row++)
        for (int col = 0; col < 80; col++) {
            fb[(btn_y + row) * fb_w + px + pw - 180 + col] = 0xFFE1E1E1;
            fb[(btn_y + row) * fb_w + px + pw - 90 + col] = 0xFFE1E1E1;
        }
    fb_set_color(0xFF000000);
    fb_draw_string(px + pw - 162, btn_y + 6, fp->mode == FPICK_OPEN ? "Open" : "Save");
    fb_draw_string(px + pw - 72, btn_y + 6, "Cancel");

    /* Border */
    for (int col = 0; col < pw; col++) {
        fb[py * fb_w + px + col] = 0xFF808080;
        fb[(py + ph - 1) * fb_w + px + col] = 0xFF808080;
    }
    for (int row = 0; row < ph; row++) {
        fb[(py + row) * fb_w + px] = 0xFF808080;
        fb[(py + row) * fb_w + px + pw - 1] = 0xFF808080;
    }
}

/* ===== App Utilities ===== */

void app_utils_center_rect(int rect_w, int rect_h, int parent_w, int parent_h,
                           int *out_x, int *out_y)
{
    if (out_x) *out_x = (parent_w - rect_w) / 2;
    if (out_y) *out_y = (parent_h - rect_h) / 2;
}

void app_utils_draw_button(uint32 *fb, int fb_w, int fb_h,
                           int x, int y, int w, int h,
                           const char *label, int pressed)
{
    if (!fb || !label) return;
    uint32 bg = pressed ? 0xFFCCE4F7 : 0xFFE1E1E1;
    uint32 border = pressed ? 0xFF0078D7 : 0xFFADADAD;

    for (int row = 0; row < h && (y + row) < fb_h; row++) {
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            if (row == 0 || row == h - 1 || col == 0 || col == w - 1)
                fb[(y + row) * fb_w + x + col] = border;
            else
                fb[(y + row) * fb_w + x + col] = bg;
        }
    }

    fb_set_color(0xFF000000);
    int label_w = strlen(label) * 8;
    int label_x = x + (w - label_w) / 2;
    int label_y = y + (h - 16) / 2;
    fb_draw_string(label_x, label_y, label);
}

void app_utils_draw_scrollbar(uint32 *fb, int fb_w, int fb_h,
                              int x, int y, int w, int h,
                              int thumb_pos, int thumb_size, int vertical)
{
    if (!fb) return;
    (void)fb_h;

    uint32 track_color = 0xFFF0F0F0;
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            fb[(y + row) * fb_w + x + col] = track_color;

    uint32 thumb_color = 0xFFC0C0C0;
    if (vertical) {
        for (int row = thumb_pos; row < thumb_pos + thumb_size && row < h; row++)
            for (int col = 2; col < w - 2; col++)
                fb[(y + row) * fb_w + x + col] = thumb_color;
    } else {
        for (int row = 2; row < h - 2; row++)
            for (int col = thumb_pos; col < thumb_pos + thumb_size && col < w; col++)
                fb[(y + row) * fb_w + x + col] = thumb_color;
    }
}

int app_utils_point_in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

void app_utils_draw_gradient(uint32 *fb, int fb_w, int x, int y, int w, int h,
                              uint32 top_color, uint32 bottom_color)
{
    if (!fb) return;
    uint8 tr = (top_color >> 16) & 0xFF, tg = (top_color >> 8) & 0xFF, tb = top_color & 0xFF;
    uint8 br = (bottom_color >> 16) & 0xFF, bg = (bottom_color >> 8) & 0xFF, bb = bottom_color & 0xFF;

    for (int row = 0; row < h && (y + row) < fb_w; row++) {
        float t = (float)row / h;
        uint8 r = (uint8)(tr + (br - tr) * t);
        uint8 g = (uint8)(tg + (bg - tg) * t);
        uint8 b = (uint8)(tb + (bb - tb) * t);
        uint32 color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            fb[(y + row) * fb_w + x + col] = color;
        }
    }
}

void app_utils_format_size(uint32 bytes, char *out, int out_len)
{
    if (!out || out_len <= 0) return;
    if (bytes < 1024)
        snprintf(out, out_len, "%u B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(out, out_len, "%u KB", bytes / 1024);
    else
        snprintf(out, out_len, "%u MB", bytes / (1024 * 1024));
}

/* ===== Timer for Apps ===== */

static app_timer_t timers[APP_MAX_TIMERS];

void app_timer_init(void)
{
    memset(timers, 0, sizeof(timers));
}

int app_timer_start(uint32 interval_ms, int repeat, void (*callback)(void *), void *arg)
{
    for (int i = 0; i < APP_MAX_TIMERS; i++) {
        if (!timers[i].active) {
            timers[i].interval = interval_ms;
            timers[i].repeat = repeat;
            timers[i].callback = callback;
            timers[i].arg = arg;
            timers[i].next_tick = get_jiffies() + interval_ms / 10;
            timers[i].active = 1;
            return i;
        }
    }
    return -1;
}

void app_timer_stop(int timer_id)
{
    if (timer_id >= 0 && timer_id < APP_MAX_TIMERS)
        timers[timer_id].active = 0;
}

void app_timer_poll_all(void)
{
    uint64 now = get_jiffies();
    for (int i = 0; i < APP_MAX_TIMERS; i++) {
        if (timers[i].active && now >= timers[i].next_tick) {
            if (timers[i].callback)
                timers[i].callback(timers[i].arg);
            if (timers[i].repeat)
                timers[i].next_tick = now + timers[i].interval / 10;
            else
                timers[i].active = 0;
        }
    }
}

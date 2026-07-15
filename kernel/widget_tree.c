/******************************************************************************
 * torOS - Terminal Operating System
 * Widget Tree - Radio Button, Checkbox, ComboBox
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/widget.h"
#include "../include/font.h"

/* ===== Radio Button ===== */

void radio_init(radio_t *radio, int x, int y, const char *label, int group_id)
{
    if (!radio) return;
    memset(radio, 0, sizeof(radio_t));
    radio->widget.type = WIDGET_RADIO;
    radio->widget.x = x;
    radio->widget.y = y;
    radio->widget.width = 120;
    radio->widget.height = 20;
    radio->widget.visible = 1;
    strncpy(radio->label, label ? label : "", RADIO_MAX_LABEL - 1);
    radio->label[RADIO_MAX_LABEL - 1] = '\0';
    radio->selected = 0;
    radio->group_id = group_id;
}

void radio_set_selected(radio_t *radio, int sel)
{
    if (!radio) return;
    radio->selected = sel;
}

int radio_is_selected(radio_t *radio)
{
    return radio ? radio->selected : 0;
}

void radio_handle_click(radio_t *radio)
{
    if (!radio) return;
    radio->selected = 1;
}

void radio_draw(radio_t *radio, uint32 *fb, int fb_w, int fb_h)
{
    if (!radio || !radio->widget.visible || !fb) return;
    int x = radio->widget.x;
    int y = radio->widget.y;

    if (x < 0 || y < 0 || x >= fb_w || y >= fb_h) return;

    /* Draw circle (12x12) */
    int cx = x + 8;
    int cy = y + 8;
    int r = 7;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int dist = dx * dx + dy * dy;
            int px = cx + dx;
            int py = cy + dy;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                if (dist <= r * r) {
                    if (dist >= (r - 1) * (r - 1))
                        fb[py * fb_w + px] = 0xFF808080; /* Border */
                    else
                        fb[py * fb_w + px] = 0xFFFFFFFF; /* Fill */
                }
            }
        }
    }

    /* Selected dot */
    if (radio->selected) {
        int dr = 3;
        for (int dy = -dr; dy <= dr; dy++) {
            for (int dx = -dr; dx <= dr; dx++) {
                if (dx * dx + dy * dy <= dr * dr)
                    fb[(cy + dy) * fb_w + cx + dx] = 0xFF0078D7;
            }
        }
    }

    /* Label */
    fb_set_color(0xFF000000);
    fb_draw_string(x + 20, y + 2, radio->label);
}

/* ===== Checkbox ===== */

void checkbox_init(checkbox_t *cb, int x, int y, const char *label)
{
    if (!cb) return;
    memset(cb, 0, sizeof(checkbox_t));
    cb->widget.type = WIDGET_CHECKBOX;
    cb->widget.x = x;
    cb->widget.y = y;
    cb->widget.width = 120;
    cb->widget.height = 20;
    cb->widget.visible = 1;
    strncpy(cb->label, label ? label : "", CB_MAX_LABEL - 1);
    cb->label[CB_MAX_LABEL - 1] = '\0';
    cb->checked = 0;
}

void checkbox_set_checked(checkbox_t *cb, int c)
{
    if (!cb) return;
    cb->checked = c;
}

int checkbox_is_checked(checkbox_t *cb)
{
    return cb ? cb->checked : 0;
}

void checkbox_toggle(checkbox_t *cb)
{
    if (!cb) return;
    cb->checked = !cb->checked;
}

void checkbox_handle_click(checkbox_t *cb)
{
    if (!cb) return;
    cb->checked = !cb->checked;
}

void checkbox_draw(checkbox_t *cb, uint32 *fb, int fb_w, int fb_h)
{
    if (!cb || !cb->widget.visible || !fb) return;
    int x = cb->widget.x;
    int y = cb->widget.y;

    if (x < 0 || y < 0 || x >= fb_w || y >= fb_h) return;

    /* Draw square (14x14) */
    for (int row = 0; row < 14; row++) {
        for (int col = 0; col < 14; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                if (row == 0 || row == 13 || col == 0 || col == 13)
                    fb[py * fb_w + px] = 0xFF808080;
                else
                    fb[py * fb_w + px] = 0xFFFFFFFF;
            }
        }
    }

    /* Checkmark */
    if (cb->checked) {
        fb_set_color(0xFF0078D7);
        /* Draw checkmark lines */
        int bx = x + 2;
        int by = y + 7;
        for (int i = 0; i < 4; i++) fb[(by + i) * fb_w + bx + i] = 0xFF0078D7;
        bx = x + 6;
        by = y + 9;
        for (int i = 0; i < 6; i++) fb[(by - i) * fb_w + bx + i] = 0xFF0078D7;
    }

    /* Label */
    fb_set_color(0xFF000000);
    fb_draw_string(x + 18, y + 2, cb->label);
}

/* ===== ComboBox ===== */

void combobox_init(combobox_t *cb, int x, int y, int w, int h)
{
    if (!cb) return;
    memset(cb, 0, sizeof(combobox_t));
    cb->widget.type = WIDGET_COMBOBOX;
    cb->widget.x = x;
    cb->widget.y = y;
    cb->widget.width = w;
    cb->widget.height = h;
    cb->widget.visible = 1;
    cb->selected_index = -1;
    cb->dropdown_open = 0;
    cb->item_height = 20;
}

int combobox_add_item(combobox_t *cb, const char *text)
{
    if (!cb || !text || cb->item_count >= CB_MAX_ITEMS) return -1;
    strncpy(cb->items[cb->item_count], text, CB_MAX_ITEM_TEXT - 1);
    cb->items[cb->item_count][CB_MAX_ITEM_TEXT - 1] = '\0';
    if (cb->selected_index < 0) cb->selected_index = 0;
    return cb->item_count++;
}

void combobox_set_selected(combobox_t *cb, int index)
{
    if (!cb || index < 0 || index >= cb->item_count) return;
    cb->selected_index = index;
}

int combobox_get_selected(combobox_t *cb)
{
    return cb ? cb->selected_index : -1;
}

const char *combobox_get_selected_text(combobox_t *cb)
{
    if (!cb || cb->selected_index < 0 || cb->selected_index >= cb->item_count)
        return NULL;
    return cb->items[cb->selected_index];
}

void combobox_toggle_dropdown(combobox_t *cb)
{
    if (!cb) return;
    cb->dropdown_open = !cb->dropdown_open;
}

void combobox_handle_item_click(combobox_t *cb, int item_index)
{
    if (!cb || item_index < 0 || item_index >= cb->item_count) return;
    cb->selected_index = item_index;
    cb->dropdown_open = 0;
}

void combobox_draw(combobox_t *cb, uint32 *fb, int fb_w, int fb_h)
{
    if (!cb || !cb->widget.visible || !fb) return;
    int x = cb->widget.x;
    int y = cb->widget.y;
    int w = cb->widget.width;
    int h = cb->widget.height;

    if (x < 0 || y < 0 || x + w > fb_w || y + h > fb_h) return;

    /* Main box background */
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            fb[(y + row) * fb_w + x + col] = 0xFFFFFFFF;
        }
    }

    /* Border */
    for (int col = 0; col < w; col++) {
        fb[y * fb_w + x + col] = 0xFF808080;
        fb[(y + h - 1) * fb_w + x + col] = 0xFF808080;
    }
    for (int row = 0; row < h; row++) {
        fb[(y + row) * fb_w + x] = 0xFF808080;
        fb[(y + row) * fb_w + x + w - 1] = 0xFF808080;
    }

    /* Selected text */
    fb_set_color(0xFF000000);
    if (cb->selected_index >= 0 && cb->selected_index < cb->item_count) {
        fb_draw_string(x + 4, y + 2, cb->items[cb->selected_index]);
    }

    /* Dropdown arrow */
    int ax = x + w - 16;
    int ay = y + h / 2;
    fb_set_color(0xFF000000);
    for (int i = 0; i < 5; i++) {
        fb[(ay - 1) * fb_w + ax + i] = 0xFF000000;
        fb[(ay) * fb_w + ax + i + 1] = 0xFF000000;
        fb[(ay + 1) * fb_w + ax + i + 2] = 0xFF000000;
    }

    /* Dropdown list */
    if (cb->dropdown_open) {
        int drop_h = cb->item_count * cb->item_height + 2;
        int drop_y = y + h;

        if (drop_y + drop_h > fb_h) drop_h = fb_h - drop_y;

        /* Dropdown background */
        for (int row = 0; row < drop_h && (drop_y + row) < fb_h; row++) {
            for (int col = 0; col < w && (x + col) < fb_w; col++) {
                fb[(drop_y + row) * fb_w + x + col] = 0xFFFFFFFF;
            }
        }

        /* Dropdown border */
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            if (drop_y < fb_h) fb[drop_y * fb_w + x + col] = 0xFF808080;
            if (drop_y + drop_h - 1 < fb_h) fb[(drop_y + drop_h - 1) * fb_w + x + col] = 0xFF808080;
        }

        /* Items */
        for (int i = 0; i < cb->item_count; i++) {
            int iy = drop_y + 1 + i * cb->item_height;
            if (iy >= fb_h) break;

            if (i == cb->selected_index) {
                for (int row = 0; row < cb->item_height && (iy + row) < fb_h; row++) {
                    for (int col = 1; col < w - 1 && (x + col) < fb_w; col++) {
                        fb[(iy + row) * fb_w + x + col] = 0xFF0078D7;
                    }
                }
                fb_set_color(0xFFFFFFFF);
            } else {
                fb_set_color(0xFF000000);
            }
            if (iy + 2 < fb_h) fb_draw_string(x + 4, iy + 2, cb->items[i]);
        }
    }
}

void combobox_clear(combobox_t *cb)
{
    if (!cb) return;
    for (int i = 0; i < cb->item_count; i++)
        cb->items[i][0] = '\0';
    cb->item_count = 0;
    cb->selected_index = -1;
    cb->dropdown_open = 0;
}

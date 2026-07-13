/*
 * torOS UI Widget Toolkit
 * Button, TextBox, ScrollBar, ListView, Menu, Dialog, Tab, ProgressBar, Tooltip
 * Checkbox, Radio, Slider, ComboBox + Label
 */

#include "../include/toros.h"
#include "../include/widget.h"

/* Static widget list per window - simplified */
static widget_t *widget_list = NULL;
static widget_t *focused_widget = NULL;
static int widget_initialized = 0;

/* ===== Draw Helpers ===== */

void widget_draw_rect(int x, int y, int w, int h, uint32 color, uint32 *fb, int fb_w, int fb_h)
{
    if (!fb) return;
    for (int row = 0; row < h && (y + row) < fb_h; row++) {
        if ((y + row) < 0) continue;
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            if ((x + col) < 0) continue;
            fb[(y + row) * fb_w + (x + col)] = color;
        }
    }
}

void widget_draw_border(int x, int y, int w, int h, uint32 color, int bw, uint32 *fb, int fb_w, int fb_h)
{
    if (!fb) return;
    for (int b = 0; b < bw; b++) {
        for (int i = 0; i < w && (x + i) < fb_w; i++) {
            if ((y + b) >= 0 && (y + b) < fb_h && (x + i) >= 0) fb[(y + b) * fb_w + x + i] = color;
            if ((y + h - 1 - b) >= 0 && (y + h - 1 - b) < fb_h && (x + i) >= 0) fb[(y + h - 1 - b) * fb_w + x + i] = color;
        }
        for (int i = 0; i < h && (y + i) < fb_h; i++) {
            if ((x + b) >= 0 && (x + b) < fb_w && (y + i) >= 0) fb[(y + i) * fb_w + x + b] = color;
            if ((x + w - 1 - b) >= 0 && (x + w - 1 - b) < fb_w && (y + i) >= 0) fb[(y + i) * fb_w + x + w - 1 - b] = color;
        }
    }
}

void widget_draw_text(int x, int y, const char *text, uint32 color, uint32 *fb, int fb_w, int fb_h)
{
    if (!fb || !text) return;
    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);
    fb_set_color(color);
    fb_draw_string(x, y, text);
}

void widget_draw_gradient(int x, int y, int w, int h, uint32 top, uint32 bottom, uint32 *fb, int fb_w, int fb_h)
{
    if (!fb) return;
    for (int row = 0; row < h && (y + row) < fb_h; row++) {
        if ((y + row) < 0) continue;
        int ratio = (row * 255) / h;
        uint8 tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
        uint8 br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;
        uint8 r = ((tr * (255 - ratio)) + (br * ratio)) / 255;
        uint8 g = ((tg * (255 - ratio)) + (bg * ratio)) / 255;
        uint8 b = ((tb * (255 - ratio)) + (bb * ratio)) / 255;
        uint32 color = 0xFF000000 | (r << 16) | (g << 8) | b;
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            if ((x + col) < 0) continue;
            fb[(y + row) * fb_w + (x + col)] = color;
        }
    }
}

void widget_draw_rounded_rect(int x, int y, int w, int h, int r, uint32 color, uint32 *fb, int fb_w, int fb_h)
{
    /* Simplified: draw normal rect (rounded corners would need circle algorithm) */
    widget_draw_rect(x, y, w, h, color, fb, fb_w, fb_h);
}

/* ===== Widget System ===== */

void widget_init(void)
{
    widget_list = NULL;
    focused_widget = NULL;
    widget_initialized = 1;
}

static void add_widget(widget_t *w)
{
    if (!w) return;
    w->next = widget_list;
    widget_list = w;
}

widget_t *widget_at_pos(window_t *win, int x, int y)
{
    (void)win;
    widget_t *w = widget_list;
    while (w) {
        if (w->visible && x >= w->x && x < w->x + w->width &&
            y >= w->y && y < w->y + w->height) {
            return w;
        }
        w = w->next;
    }
    return NULL;
}

void widget_set_focus(widget_t *w)
{
    if (focused_widget) focused_widget->focused = 0;
    focused_widget = w;
    if (w) w->focused = 1;
}

void widget_invalidate(widget_t *w)
{
    if (w && w->parent) wm_invalidate_window(w->parent);
}

int widget_handle_mouse(window_t *win, int x, int y, int button, int pressed)
{
    (void)win;
    widget_t *w = widget_at_pos(win, x, y);
    if (!w) return 0;

    widget_set_focus(w);

    switch (w->type) {
    case WIDGET_BUTTON: {
        button_widget_t *btn = (button_widget_t *)w;
        btn->pressed = pressed;
        if (!pressed && btn->base.on_click) btn->base.on_click(w, x, y);
        if (btn->toggle_mode && !pressed) btn->toggled = !btn->toggled;
        widget_invalidate(w);
        return 1;
    }
    case WIDGET_CHECKBOX: {
        checkbox_widget_t *cbx = (checkbox_widget_t *)w;
        if (!pressed) { cbx->checked = !cbx->checked; widget_invalidate(w); }
        return 1;
    }
    case WIDGET_SCROLLBAR: {
        scrollbar_widget_t *sb = (scrollbar_widget_t *)w;
        if (pressed) { sb->dragging = 1; sb->drag_start = sb->orientation ? x : y; }
        else sb->dragging = 0;
        return 1;
    }
    case WIDGET_SLIDER: {
        slider_widget_t *sld = (slider_widget_t *)w;
        if (pressed) sld->dragging = 1; else sld->dragging = 0;
        return 1;
    }
    case WIDGET_COMBOBOX: {
        combobox_widget_t *cbo = (combobox_widget_t *)w;
        if (!pressed) { cbo->dropped = !cbo->dropped; widget_invalidate(w); }
        return 1;
    }
    default:
        if (!pressed && w->on_click) w->on_click(w, x, y);
        return 1;
    }
}

int widget_handle_key(window_t *win, uint16 keycode)
{
    (void)win;
    if (!focused_widget) return 0;
    if (focused_widget->on_key) return focused_widget->on_key(focused_widget, keycode);

    switch (focused_widget->type) {
    case WIDGET_TEXTBOX:
        tb_handle_key((textbox_widget_t *)focused_widget, keycode);
        return 1;
    default:
        return 0;
    }
}

/* ===== Button ===== */

button_widget_t *btn_create(window_t *parent, int x, int y, int w, int h, const char *text)
{
    button_widget_t *btn = (button_widget_t *)kmalloc(sizeof(button_widget_t));
    if (!btn) return NULL;
    memset(btn, 0, sizeof(button_widget_t));
    btn->base.type = WIDGET_BUTTON;
    btn->base.state = STATE_NORMAL;
    btn->base.x = x; btn->base.y = y;
    btn->base.width = w; btn->base.height = h;
    btn->base.bg_color = WIDGET_BG_NORMAL;
    btn->base.fg_color = WIDGET_TEXT_NORMAL;
    btn->base.border_color = WIDGET_BORDER_NORMAL;
    btn->base.visible = 1;
    btn->base.enabled = 1;
    btn->base.parent = parent;
    strncpy(btn->base.text, text ? text : "", 255);
    btn->base.text[255] = '\0';
    btn->press_color = WIDGET_BG_PRESSED;
    btn->border_radius = 2;
    add_widget(&btn->base);
    return btn;
}

void btn_draw(button_widget_t *btn)
{
    if (!btn || !btn->base.visible) return;
    window_t *win = btn->base.parent;
    if (!win || !win->framebuffer) return;
    int x = btn->base.x, y = btn->base.y, w = btn->base.width, h = btn->base.height;
    uint32 *fb = win->framebuffer;
    int fb_w = win->full_width, fb_h = win->full_height;

    uint32 bg = btn->base.bg_color;
    if (btn->pressed || (btn->toggle_mode && btn->toggled)) bg = btn->press_color;
    else if (btn->base.state == STATE_HOVER) bg = WIDGET_BG_HOVER;

    widget_draw_gradient(x, y, w, h, bg, bg, fb, fb_w, fb_h);
    widget_draw_border(x, y, w, h, btn->base.border_color, 1, fb, fb_w, fb_h);

    /* Text centered */
    int tx = x + (w - strlen(btn->base.text) * 8) / 2;
    int ty = y + (h - 8) / 2;
    widget_draw_text(tx, ty, btn->base.text, btn->base.fg_color, fb, fb_w, fb_h);
}

void btn_set_text(button_widget_t *btn, const char *text)
{ if (btn) { strncpy(btn->base.text, text ? text : "", 255); widget_invalidate(&btn->base); } }
void btn_set_toggle(button_widget_t *btn, int toggle)
{ if (btn) btn->toggle_mode = toggle; }
int btn_get_toggle(button_widget_t *btn)
{ return btn ? btn->toggled : 0; }
void btn_set_colors(button_widget_t *btn, uint32 normal, uint32 hover, uint32 pressed)
{ if (btn) { btn->base.bg_color = normal; btn->press_color = pressed; } }

/* ===== Label ===== */

label_widget_t *lbl_create(window_t *parent, int x, int y, int w, int h, const char *text)
{
    label_widget_t *lbl = (label_widget_t *)kmalloc(sizeof(label_widget_t));
    if (!lbl) return NULL;
    memset(lbl, 0, sizeof(label_widget_t));
    lbl->base.type = WIDGET_LABEL;
    lbl->base.x = x; lbl->base.y = y;
    lbl->base.width = w; lbl->base.height = h;
    lbl->base.fg_color = WIDGET_TEXT_NORMAL;
    lbl->base.visible = 1;
    lbl->base.parent = parent;
    strncpy(lbl->base.text, text ? text : "", 255);
    lbl->align = 0; lbl->valign = 0; lbl->font_size = 8;
    add_widget(&lbl->base);
    return lbl;
}

void lbl_draw(label_widget_t *lbl)
{
    if (!lbl || !lbl->base.visible) return;
    window_t *win = lbl->base.parent;
    if (!win || !win->framebuffer) return;
    int tx = lbl->base.x;
    if (lbl->align == 1) tx += (lbl->base.width - strlen(lbl->base.text) * 8) / 2;
    else if (lbl->align == 2) tx += lbl->base.width - strlen(lbl->base.text) * 8;
    int ty = lbl->base.y;
    if (lbl->valign == 1) ty += (lbl->base.height - 8) / 2;
    else if (lbl->valign == 2) ty += lbl->base.height - 8;
    widget_draw_text(tx, ty, lbl->base.text, lbl->base.fg_color, win->framebuffer, win->full_width, win->full_height);
}

void lbl_set_text(label_widget_t *lbl, const char *text)
{ if (lbl) { strncpy(lbl->base.text, text ? text : "", 255); widget_invalidate(&lbl->base); } }
void lbl_set_align(label_widget_t *lbl, int halign, int valign)
{ if (lbl) { lbl->align = halign; lbl->valign = valign; } }

/* ===== TextBox ===== */

textbox_widget_t *tb_create(window_t *parent, int x, int y, int w, int h)
{
    textbox_widget_t *tb = (textbox_widget_t *)kmalloc(sizeof(textbox_widget_t));
    if (!tb) return NULL;
    memset(tb, 0, sizeof(textbox_widget_t));
    tb->base.type = WIDGET_TEXTBOX;
    tb->base.x = x; tb->base.y = y;
    tb->base.width = w; tb->base.height = h;
    tb->base.bg_color = 0xFFFFFFFF;
    tb->base.fg_color = WIDGET_TEXT_NORMAL;
    tb->base.border_color = WIDGET_BORDER_FOCUSED;
    tb->base.visible = 1;
    tb->base.enabled = 1;
    tb->base.parent = parent;
    tb->max_length = TEXTBOX_MAX_LEN - 1;
    tb->cursor_pos = 0;
    tb->multiline = 0;
    tb->password_mode = 0;
    tb->read_only = 0;
    add_widget(&tb->base);
    return tb;
}

void tb_draw(textbox_widget_t *tb)
{
    if (!tb || !tb->base.visible) return;
    window_t *win = tb->base.parent;
    if (!win || !win->framebuffer) return;
    int x = tb->base.x, y = tb->base.y, w = tb->base.width, h = tb->base.height;
    uint32 *fb = win->framebuffer;
    int fb_w = win->full_width;

    /* Background */
    widget_draw_rect(x, y, w, h, tb->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, tb->base.border_color, 1, fb, fb_w, win->full_height);

    /* Draw text or password dots */
    if (tb->password_mode) {
        char dots[64]; int len = strlen(tb->buffer); if (len > 60) len = 60;
        for (int i = 0; i < len; i++) dots[i] = '*'; dots[len] = '\0';
        widget_draw_text(x + 4, y + (h - 8) / 2, dots, tb->base.fg_color, fb, fb_w, win->full_height);
    } else {
        widget_draw_text(x + 4, y + (h - 8) / 2, tb->buffer, tb->base.fg_color, fb, fb_w, win->full_height);
    }

    /* Cursor */
    if (tb->base.focused) {
        int cx = x + 4 + tb->cursor_pos * 8;
        if (cx < x + w - 4) {
            for (int cy = y + 3; cy < y + h - 3; cy++)
                fb[cy * fb_w + cx] = 0xFF000000;
        }
    }
}

void tb_set_text(textbox_widget_t *tb, const char *text)
{ if (tb && text) { strncpy(tb->buffer, text, tb->max_length); tb->buffer[tb->max_length] = '\0'; tb->cursor_pos = strlen(tb->buffer); widget_invalidate(&tb->base); } }
const char *tb_get_text(textbox_widget_t *tb) { return tb ? tb->buffer : ""; }
void tb_set_password(textbox_widget_t *tb, int p) { if (tb) tb->password_mode = p; }
void tb_set_multiline(textbox_widget_t *tb, int m) { if (tb) tb->multiline = m; }
void tb_set_readonly(textbox_widget_t *tb, int ro) { if (tb) tb->read_only = ro; }

void tb_insert(textbox_widget_t *tb, const char *text)
{
    if (!tb || !text || tb->read_only) return;
    int len = strlen(tb->buffer);
    int tlen = strlen(text);
    if (len + tlen > tb->max_length) tlen = tb->max_length - len;
    memmove(tb->buffer + tb->cursor_pos + tlen, tb->buffer + tb->cursor_pos, len - tb->cursor_pos + 1);
    memcpy(tb->buffer + tb->cursor_pos, text, tlen);
    tb->cursor_pos += tlen;
    widget_invalidate(&tb->base);
}

void tb_handle_key(textbox_widget_t *tb, uint16 keycode)
{
    if (!tb || tb->read_only) return;
    switch (keycode) {
    case KEY_LEFT:
        if (tb->cursor_pos > 0) tb->cursor_pos--;
        break;
    case KEY_RIGHT:
        if (tb->cursor_pos < (int)strlen(tb->buffer)) tb->cursor_pos++;
        break;
    case KEY_HOME:
        tb->cursor_pos = 0;
        break;
    case KEY_END:
        tb->cursor_pos = strlen(tb->buffer);
        break;
    case KEY_BACKSPACE:
        if (tb->cursor_pos > 0) {
            memmove(tb->buffer + tb->cursor_pos - 1, tb->buffer + tb->cursor_pos,
                    strlen(tb->buffer) - tb->cursor_pos + 1);
            tb->cursor_pos--;
        }
        break;
    case KEY_DELETE:
        if (tb->cursor_pos < (int)strlen(tb->buffer)) {
            memmove(tb->buffer + tb->cursor_pos, tb->buffer + tb->cursor_pos + 1,
                    strlen(tb->buffer) - tb->cursor_pos);
        }
        break;
    case KEY_ENTER:
        if (tb->multiline) tb_insert(tb, "\n");
        else if (tb->base.on_change) tb->base.on_change(&tb->base);
        break;
    default:
        if (keycode >= KEY_SPACE && keycode <= KEY_Z) {
            char ch = (char)keycode;
            if (keycode >= KEY_A && keycode <= KEY_Z) ch = ch - KEY_A + 'a';
            else if (keycode >= KEY_1 && keycode <= KEY_9) ch = ch - KEY_1 + '1';
            else if (keycode == KEY_0) ch = '0';
            else if (keycode == KEY_SPACE) ch = ' ';
            char str[2] = {ch, '\0'};
            tb_insert(tb, str);
        }
        break;
    }
    widget_invalidate(&tb->base);
}

/* ===== ScrollBar ===== */

scrollbar_widget_t *sb_create(window_t *parent, int x, int y, int w, int h, int vertical)
{
    scrollbar_widget_t *sb = (scrollbar_widget_t *)kmalloc(sizeof(scrollbar_widget_t));
    if (!sb) return NULL;
    memset(sb, 0, sizeof(scrollbar_widget_t));
    sb->base.type = WIDGET_SCROLLBAR;
    sb->base.x = x; sb->base.y = y;
    sb->base.width = w; sb->base.height = h;
    sb->base.bg_color = 0xFFF0F0F0;
    sb->base.visible = 1;
    sb->base.parent = parent;
    sb->orientation = vertical;
    sb->min_val = 0; sb->max_val = 100; sb->current_val = 0; sb->page_size = 10;
    sb->thumb_size = vertical ? (h / 5) : (w / 5); if (sb->thumb_size < 16) sb->thumb_size = 16;
    add_widget(&sb->base);
    return sb;
}

void sb_draw(scrollbar_widget_t *sb)
{
    if (!sb || !sb->base.visible) return;
    window_t *win = sb->base.parent;
    if (!win || !win->framebuffer) return;
    int x = sb->base.x, y = sb->base.y, w = sb->base.width, h = sb->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    widget_draw_rect(x, y, w, h, sb->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    /* Thumb */
    int range = sb->max_val - sb->min_val;
    if (range <= 0) return;
    int track = sb->orientation ? (h - sb->thumb_size) : (w - sb->thumb_size);
    int pos = track * (sb->current_val - sb->min_val) / range;
    if (sb->orientation)
        widget_draw_rect(x + 1, y + pos, w - 2, sb->thumb_size, WIDGET_ACCENT, fb, fb_w, win->full_height);
    else
        widget_draw_rect(x + pos, y + 1, sb->thumb_size, h - 2, WIDGET_ACCENT, fb, fb_w, win->full_height);
}

void sb_set_range(scrollbar_widget_t *sb, int min, int max, int page)
{ if (sb) { sb->min_val = min; sb->max_val = max; sb->page_size = page; widget_invalidate(&sb->base); } }
void sb_set_value(scrollbar_widget_t *sb, int val)
{ if (sb) { if (val < sb->min_val) val = sb->min_val; if (val > sb->max_val) val = sb->max_val; sb->current_val = val; widget_invalidate(&sb->base); } }
int sb_get_value(scrollbar_widget_t *sb) { return sb ? sb->current_val : 0; }

/* ===== ListView ===== */

listview_widget_t *lv_create(window_t *parent, int x, int y, int w, int h)
{
    listview_widget_t *lv = (listview_widget_t *)kmalloc(sizeof(listview_widget_t));
    if (!lv) return NULL;
    memset(lv, 0, sizeof(listview_widget_t));
    lv->base.type = WIDGET_LISTVIEW;
    lv->base.x = x; lv->base.y = y;
    lv->base.width = w; lv->base.height = h;
    lv->base.bg_color = 0xFFFFFFFF;
    lv->base.visible = 1;
    lv->base.parent = parent;
    lv->header_height = 20;
    lv->show_headers = 1;
    lv->columns = 1;
    lv->col_widths[0] = w;
    add_widget(&lv->base);
    return lv;
}

void lv_draw(listview_widget_t *lv)
{
    if (!lv || !lv->base.visible) return;
    window_t *win = lv->base.parent;
    if (!win || !win->framebuffer) return;
    int x = lv->base.x, y = lv->base.y, w = lv->base.width, h = lv->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    /* Background */
    widget_draw_rect(x, y, w, h, lv->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    /* Headers */
    if (lv->show_headers) {
        widget_draw_rect(x, y, w, lv->header_height, 0xFFE0E0E0, fb, fb_w, win->full_height);
        int cx = x;
        for (int c = 0; c < lv->columns; c++) {
            widget_draw_text(cx + 4, y + 4, lv->headers[c], WIDGET_TEXT_NORMAL, fb, fb_w, win->full_height);
            cx += lv->col_widths[c];
        }
    }

    /* Items */
    int item_h = 18;
    int start_y = y + (lv->show_headers ? lv->header_height : 0);
    for (int i = lv->top_idx; i < lv->item_count && (start_y + (i - lv->top_idx) * item_h) < y + h - item_h; i++) {
        int iy = start_y + (i - lv->top_idx) * item_h;
        uint32 bg = (i == lv->selected_idx) ? WIDGET_SELECTION_BG : lv->base.bg_color;
        uint32 fg = (i == lv->selected_idx) ? WIDGET_SELECTION_TEXT : WIDGET_TEXT_NORMAL;
        widget_draw_rect(x + 1, iy, w - 2, item_h, bg, fb, fb_w, win->full_height);
        widget_draw_text(x + 4, iy + 2, lv->items[i].text, fg, fb, fb_w, win->full_height);
    }
}

void lv_add_item(listview_widget_t *lv, const char *text)
{
    if (!lv || lv->item_count >= LV_MAX_ITEMS) return;
    strncpy(lv->items[lv->item_count].text, text ? text : "", LV_ITEM_LEN - 1);
    lv->items[lv->item_count].text[LV_ITEM_LEN - 1] = '\0';
    lv->item_count++;
    widget_invalidate(&lv->base);
}

void lv_clear(listview_widget_t *lv)
{ if (lv) { lv->item_count = 0; lv->selected_idx = -1; lv->top_idx = 0; widget_invalidate(&lv->base); } }
void lv_set_columns(listview_widget_t *lv, int cols, int widths[])
{ if (lv) { lv->columns = cols; for (int i = 0; i < cols && i < LV_MAX_COLS; i++) lv->col_widths[i] = widths[i]; } }
void lv_set_headers(listview_widget_t *lv, char *headers[])
{ if (lv) for (int i = 0; i < lv->columns && i < LV_MAX_COLS; i++) strncpy(lv->headers[i], headers[i], LV_ITEM_LEN - 1); }
int lv_get_selected(listview_widget_t *lv) { return lv ? lv->selected_idx : -1; }
const char *lv_get_item(listview_widget_t *lv, int idx) { return (lv && idx >= 0 && idx < lv->item_count) ? lv->items[idx].text : ""; }

/* ===== Menu ===== */

menu_widget_t *menu_create(window_t *parent, int x, int y, int w, int h)
{
    menu_widget_t *menu = (menu_widget_t *)kmalloc(sizeof(menu_widget_t));
    if (!menu) return NULL;
    memset(menu, 0, sizeof(menu_widget_t));
    menu->base.type = WIDGET_MENU;
    menu->base.x = x; menu->base.y = y;
    menu->base.width = w; menu->base.height = h;
    menu->base.bg_color = 0xFFFFFFFF;
    menu->base.visible = 0;
    menu->base.parent = parent;
    menu->item_height = 24;
    add_widget(&menu->base);
    return menu;
}

void menu_draw(menu_widget_t *menu)
{
    if (!menu || !menu->base.visible || !menu->open) return;
    window_t *win = menu->base.parent;
    if (!win || !win->framebuffer) return;
    int x = menu->base.x, y = menu->base.y, w = menu->base.width;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;
    int total_h = menu->item_count * menu->item_height;

    /* Background + border */
    widget_draw_rect(x, y, w, total_h, menu->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, total_h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    for (int i = 0; i < menu->item_count; i++) {
        int iy = y + i * menu->item_height;
        if (menu->items[i].separator) {
            widget_draw_rect(x + 10, iy + menu->item_height / 2, w - 20, 1, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);
        } else {
            uint32 bg = (i == menu->selected_idx) ? WIDGET_SELECTION_BG : menu->base.bg_color;
            uint32 fg = (i == menu->selected_idx) ? WIDGET_SELECTION_TEXT : (menu->items[i].enabled ? WIDGET_TEXT_NORMAL : WIDGET_TEXT_DISABLED);
            widget_draw_rect(x + 1, iy + 1, w - 2, menu->item_height - 1, bg, fb, fb_w, win->full_height);
            widget_draw_text(x + 8, iy + 4, menu->items[i].text, fg, fb, fb_w, win->full_height);
        }
    }
}

void menu_add_item(menu_widget_t *menu, const char *text, widget_click_cb cb)
{
    if (!menu || menu->item_count >= MENU_MAX_ITEMS) return;
    strncpy(menu->items[menu->item_count].text, text ? text : "", MENU_ITEM_LEN - 1);
    menu->items[menu->item_count].enabled = 1;
    menu->items[menu->item_count].separator = 0;
    menu->items[menu->item_count].on_click = cb;
    menu->item_count++;
}

void menu_add_separator(menu_widget_t *menu)
{
    if (!menu || menu->item_count >= MENU_MAX_ITEMS) return;
    menu->items[menu->item_count].separator = 1;
    menu->item_count++;
}

void menu_show(menu_widget_t *menu, int x, int y)
{ if (menu) { menu->base.x = x; menu->base.y = y; menu->open = 1; menu->base.visible = 1; widget_invalidate(&menu->base); } }
void menu_hide(menu_widget_t *menu)
{ if (menu) { menu->open = 0; menu->base.visible = 0; widget_invalidate(&menu->base); } }

/* ===== Dialog ===== */

dialog_widget_t *dlg_create(const char *title, const char *message, int modal)
{
    dialog_widget_t *dlg = (dialog_widget_t *)kmalloc(sizeof(dialog_widget_t));
    if (!dlg) return NULL;
    memset(dlg, 0, sizeof(dialog_widget_t));
    dlg->base.type = WIDGET_DIALOG;
    dlg->base.visible = 0;
    dlg->modal = modal;
    strncpy(dlg->title, title ? title : "", DLG_TITLE_LEN - 1);
    strncpy(dlg->message, message ? message : "", DLG_MSG_LEN - 1);
    dlg->result = -1;
    dlg->button_count = 0;
    add_widget(&dlg->base);
    return dlg;
}

void dlg_draw(dialog_widget_t *dlg)
{
    if (!dlg || !dlg->base.visible) return;
    /* Dialog draws on top of compose buffer - simplified */
    int cx = (FB_WIDTH - 400) / 2, cy = (FB_HEIGHT - 200) / 2;
    uint32 *fb = NULL;
    extern uint32 *compositor_get_buffer(void);
    fb = compositor_get_buffer();
    if (!fb) return;

    /* Dim background */
    for (int y = 0; y < FB_HEIGHT; y++)
        for (int x = 0; x < FB_WIDTH; x++)
            fb[y * FB_WIDTH + x] = (((fb[y * FB_WIDTH + x] >> 1) & 0x7F7F7F7F) | 0xFF000000);

    /* Dialog box */
    widget_draw_rect(cx, cy, 400, 200, WIDGET_DIALOG_BG, fb, FB_WIDTH, FB_HEIGHT);
    widget_draw_border(cx, cy, 400, 200, WIDGET_BORDER_NORMAL, 1, fb, FB_WIDTH, FB_HEIGHT);
    widget_draw_rect(cx, cy, 400, 28, WIDGET_DIALOG_TITLE, fb, FB_WIDTH, FB_HEIGHT);
    widget_draw_text(cx + 8, cy + 6, dlg->title, 0xFFFFFFFF, fb, FB_WIDTH, FB_HEIGHT);
    widget_draw_text(cx + 20, cy + 50, dlg->message, WIDGET_TEXT_NORMAL, fb, FB_WIDTH, FB_HEIGHT);

    /* Buttons */
    for (int i = 0; i < dlg->button_count; i++) {
        int bx = cx + 400 - (dlg->button_count - i) * 90;
        int by = cy + 200 - 40;
        widget_draw_rect(bx, by, 80, 28, WIDGET_BG_NORMAL, fb, FB_WIDTH, FB_HEIGHT);
        widget_draw_border(bx, by, 80, 28, WIDGET_BORDER_NORMAL, 1, fb, FB_WIDTH, FB_HEIGHT);
        widget_draw_text(bx + 8, by + 6, dlg->buttons[i].base.text, WIDGET_TEXT_NORMAL, fb, FB_WIDTH, FB_HEIGHT);
    }
}

void dlg_add_button(dialog_widget_t *dlg, const char *text, int result)
{
    if (!dlg || dlg->button_count >= DLG_MAX_BUTTONS) return;
    memset(&dlg->buttons[dlg->button_count], 0, sizeof(button_widget_t));
    dlg->buttons[dlg->button_count].base.type = WIDGET_BUTTON;
    strncpy(dlg->buttons[dlg->button_count].base.text, text ? text : "", 255);
    dlg->buttons[dlg->button_count].base.text[255] = '\0';
    dlg->button_count++;
    (void)result;
}

int dlg_show_modal(dialog_widget_t *dlg)
{
    if (!dlg) return -1;
    dlg->base.visible = 1;
    dlg->result = -1;
    dlg_draw(dlg);
    /* In real implementation: event loop here */
    return dlg->result;
}

void dlg_close(dialog_widget_t *dlg, int result)
{ if (dlg) { dlg->result = result; dlg->base.visible = 0; } }

/* ===== Tab ===== */

tab_widget_t *tab_create(window_t *parent, int x, int y, int w, int h)
{
    tab_widget_t *tab = (tab_widget_t *)kmalloc(sizeof(tab_widget_t));
    if (!tab) return NULL;
    memset(tab, 0, sizeof(tab_widget_t));
    tab->base.type = WIDGET_TAB;
    tab->base.x = x; tab->base.y = y;
    tab->base.width = w; tab->base.height = h;
    tab->base.bg_color = 0xFFF0F0F0;
    tab->base.visible = 1;
    tab->base.parent = parent;
    tab->tab_height = 28;
    add_widget(&tab->base);
    return tab;
}

void tab_draw(tab_widget_t *tab)
{
    if (!tab || !tab->base.visible) return;
    window_t *win = tab->base.parent;
    if (!win || !win->framebuffer) return;
    int x = tab->base.x, y = tab->base.y, w = tab->base.width, h = tab->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    /* Tab bar background */
    widget_draw_rect(x, y, w, tab->tab_height, 0xFFE0E0E0, fb, fb_w, win->full_height);

    /* Individual tabs */
    int tx = x;
    for (int i = 0; i < tab->tab_count; i++) {
        int tw = 80;
        uint32 bg = (i == tab->active_tab) ? tab->base.bg_color : 0xFFD0D0D0;
        uint32 fg = WIDGET_TEXT_NORMAL;

        widget_draw_rect(tx, y, tw, tab->tab_height, bg, fb, fb_w, win->full_height);
        widget_draw_border(tx, y, tw, tab->tab_height, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);
        widget_draw_text(tx + 8, y + 6, tab->tabs[i].label, fg, fb, fb_w, win->full_height);
        tx += tw;
    }

    /* Content area */
    widget_draw_rect(x, y + tab->tab_height, w, h - tab->tab_height, tab->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y + tab->tab_height, w, h - tab->tab_height, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);
}

int tab_add(tab_widget_t *tab, const char *label)
{
    if (!tab || tab->tab_count >= TAB_MAX_TABS) return -1;
    strncpy(tab->tabs[tab->tab_count].label, label ? label : "Tab", TAB_LABEL_LEN - 1);
    tab->tabs[tab->tab_count].active = 0;
    tab->tabs[tab->tab_count].content = NULL;
    if (tab->tab_count == 0) tab->tabs[0].active = 1;
    return tab->tab_count++;
}

void tab_set_active(tab_widget_t *tab, int idx)
{ if (tab && idx >= 0 && idx < tab->tab_count) { tab->active_tab = idx; widget_invalidate(&tab->base); } }
int tab_get_active(tab_widget_t *tab) { return tab ? tab->active_tab : 0; }

/* ===== ProgressBar ===== */

progressbar_widget_t *pb_create(window_t *parent, int x, int y, int w, int h)
{
    progressbar_widget_t *pb = (progressbar_widget_t *)kmalloc(sizeof(progressbar_widget_t));
    if (!pb) return NULL;
    memset(pb, 0, sizeof(progressbar_widget_t));
    pb->base.type = WIDGET_PROGRESSBAR;
    pb->base.x = x; pb->base.y = y;
    pb->base.width = w; pb->base.height = h;
    pb->base.bg_color = 0xFFE0E0E0;
    pb->base.visible = 1;
    pb->base.parent = parent;
    pb->min_val = 0; pb->max_val = 100; pb->current_val = 0;
    pb->show_percent = 1;
    pb->bar_color = WIDGET_ACCENT;
    add_widget(&pb->base);
    return pb;
}

void pb_draw(progressbar_widget_t *pb)
{
    if (!pb || !pb->base.visible) return;
    window_t *win = pb->base.parent;
    if (!win || !win->framebuffer) return;
    int x = pb->base.x, y = pb->base.y, w = pb->base.width, h = pb->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    /* Background */
    widget_draw_rect(x, y, w, h, pb->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    /* Bar */
    int range = pb->max_val - pb->min_val;
    if (range > 0) {
        int bar_w = (w - 4) * (pb->current_val - pb->min_val) / range;
        widget_draw_gradient(x + 2, y + 2, bar_w, h - 4, pb->bar_color, pb->bar_color, fb, fb_w, win->full_height);
    }

    /* Percent text */
    if (pb->show_percent && range > 0) {
        char pct[8];
        int val = 100 * (pb->current_val - pb->min_val) / range;
        utoa(val, pct, 10);
        strcat(pct, "%");
        int tx = x + (w - strlen(pct) * 8) / 2;
        int ty = y + (h - 8) / 2;
        widget_draw_text(tx, ty, pct, WIDGET_TEXT_NORMAL, fb, fb_w, win->full_height);
    }
}

void pb_set_range(progressbar_widget_t *pb, int min, int max)
{ if (pb) { pb->min_val = min; pb->max_val = max; widget_invalidate(&pb->base); } }
void pb_set_value(progressbar_widget_t *pb, int val)
{ if (pb) { pb->current_val = val; widget_invalidate(&pb->base); } }
int pb_get_value(progressbar_widget_t *pb) { return pb ? pb->current_val : 0; }

/* ===== Tooltip ===== */

tooltip_widget_t *tt_create(window_t *parent)
{
    tooltip_widget_t *tt = (tooltip_widget_t *)kmalloc(sizeof(tooltip_widget_t));
    if (!tt) return NULL;
    memset(tt, 0, sizeof(tooltip_widget_t));
    tt->base.type = WIDGET_TOOLTIP;
    tt->base.visible = 0;
    tt->base.parent = parent;
    tt->auto_hide_ms = 3000;
    add_widget(&tt->base);
    return tt;
}

void tt_draw(tooltip_widget_t *tt)
{
    if (!tt || !tt->base.visible) return;
    /* Auto-hide */
    if (tt->auto_hide_ms > 0 && (get_jiffies() - tt->show_time) > (uint64)(tt->auto_hide_ms / 10)) {
        tt->base.visible = 0; return;
    }
    window_t *win = tt->base.parent;
    if (!win || !win->framebuffer) return;
    int x = tt->base.x, y = tt->base.y;
    int tw = strlen(tt->tooltip_text) * 8 + 12;
    int th = 24;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    widget_draw_rect(x, y, tw, th, WIDGET_TOOLTIP_BG, fb, fb_w, win->full_height);
    widget_draw_border(x, y, tw, th, WIDGET_TOOLTIP_BORDER, 1, fb, fb_w, win->full_height);
    widget_draw_text(x + 6, y + 4, tt->tooltip_text, WIDGET_TEXT_NORMAL, fb, fb_w, win->full_height);
}

void tt_show(tooltip_widget_t *tt, const char *text, int x, int y)
{
    if (!tt || !text) return;
    strncpy(tt->tooltip_text, text, 255);
    tt->tooltip_text[255] = '\0';
    tt->base.x = x; tt->base.y = y;
    tt->base.visible = 1;
    tt->show_time = get_jiffies();
    widget_invalidate(&tt->base);
}

void tt_hide(tooltip_widget_t *tt) { if (tt) tt->base.visible = 0; }
void tt_set_delay(tooltip_widget_t *tt, int ms) { if (tt) tt->auto_hide_ms = ms; }

/* ===== Checkbox ===== */

checkbox_widget_t *cbx_create(window_t *parent, int x, int y, int w, int h, const char *text)
{
    checkbox_widget_t *cbx = (checkbox_widget_t *)kmalloc(sizeof(checkbox_widget_t));
    if (!cbx) return NULL;
    memset(cbx, 0, sizeof(checkbox_widget_t));
    cbx->base.type = WIDGET_CHECKBOX;
    cbx->base.x = x; cbx->base.y = y;
    cbx->base.width = w; cbx->base.height = h;
    cbx->base.fg_color = WIDGET_TEXT_NORMAL;
    cbx->base.visible = 1;
    cbx->base.parent = parent;
    cbx->box_size = 16;
    strncpy(cbx->base.text, text ? text : "", 255);
    add_widget(&cbx->base);
    return cbx;
}

void cbx_draw(checkbox_widget_t *cbx)
{
    if (!cbx || !cbx->base.visible) return;
    window_t *win = cbx->base.parent;
    if (!win || !win->framebuffer) return;
    int x = cbx->base.x, y = cbx->base.y, s = cbx->box_size;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    widget_draw_rect(x, y, s, s, 0xFFFFFFFF, fb, fb_w, win->full_height);
    widget_draw_border(x, y, s, s, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    if (cbx->checked) {
        /* Draw checkmark */
        for (int i = 3; i < s - 3; i++) {
            fb[(y + s - 3 - i) * fb_w + x + i] = WIDGET_ACCENT;
            fb[(y + s - 4 - i) * fb_w + x + i] = WIDGET_ACCENT;
        }
    }
    widget_draw_text(x + s + 4, y + 2, cbx->base.text, cbx->base.fg_color, fb, fb_w, win->full_height);
}

void cbx_set_checked(checkbox_widget_t *cbx, int checked)
{ if (cbx) { cbx->checked = checked; widget_invalidate(&cbx->base); } }
int cbx_get_checked(checkbox_widget_t *cbx) { return cbx ? cbx->checked : 0; }

/* ===== Slider ===== */

slider_widget_t *sld_create(window_t *parent, int x, int y, int w, int h, int vertical)
{
    slider_widget_t *sld = (slider_widget_t *)kmalloc(sizeof(slider_widget_t));
    if (!sld) return NULL;
    memset(sld, 0, sizeof(slider_widget_t));
    sld->base.type = WIDGET_SLIDER;
    sld->base.x = x; sld->base.y = y;
    sld->base.width = w; sld->base.height = h;
    sld->base.bg_color = 0xFFE0E0E0;
    sld->base.visible = 1;
    sld->base.parent = parent;
    sld->orientation = vertical;
    sld->min_val = 0; sld->max_val = 100; sld->current_val = 50;
    sld->thumb_size = 16;
    add_widget(&sld->base);
    return sld;
}

void sld_draw(slider_widget_t *sld)
{
    if (!sld || !sld->base.visible) return;
    window_t *win = sld->base.parent;
    if (!win || !win->framebuffer) return;
    int x = sld->base.x, y = sld->base.y, w = sld->base.width, h = sld->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    widget_draw_rect(x, y, w, h, sld->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    int range = sld->max_val - sld->min_val;
    if (range <= 0) return;

    if (sld->orientation) {
        int track = h - sld->thumb_size;
        int pos = track * (sld->current_val - sld->min_val) / range;
        widget_draw_rect(x + 2, y + pos, w - 4, sld->thumb_size, WIDGET_ACCENT, fb, fb_w, win->full_height);
    } else {
        int track = w - sld->thumb_size;
        int pos = track * (sld->current_val - sld->min_val) / range;
        widget_draw_rect(x + pos, y + 2, sld->thumb_size, h - 4, WIDGET_ACCENT, fb, fb_w, win->full_height);
    }
}

void sld_set_range(slider_widget_t *sld, int min, int max)
{ if (sld) { sld->min_val = min; sld->max_val = max; widget_invalidate(&sld->base); } }
void sld_set_value(slider_widget_t *sld, int val)
{ if (sld) { if (val < sld->min_val) val = sld->min_val; if (val > sld->max_val) val = sld->max_val; sld->current_val = val; widget_invalidate(&sld->base); } }
int sld_get_value(slider_widget_t *sld) { return sld ? sld->current_val : 0; }

/* ===== ComboBox ===== */

combobox_widget_t *cbo_create(window_t *parent, int x, int y, int w, int h)
{
    combobox_widget_t *cbo = (combobox_widget_t *)kmalloc(sizeof(combobox_widget_t));
    if (!cbo) return NULL;
    memset(cbo, 0, sizeof(combobox_widget_t));
    cbo->base.type = WIDGET_COMBOBOX;
    cbo->base.x = x; cbo->base.y = y;
    cbo->base.width = w; cbo->base.height = h;
    cbo->base.bg_color = 0xFFFFFFFF;
    cbo->base.visible = 1;
    cbo->base.parent = parent;
    cbo->dropped = 0;
    cbo->drop_height = 120;
    add_widget(&cbo->base);
    return cbo;
}

void cbo_draw(combobox_widget_t *cbo)
{
    if (!cbo || !cbo->base.visible) return;
    window_t *win = cbo->base.parent;
    if (!win || !win->framebuffer) return;
    int x = cbo->base.x, y = cbo->base.y, w = cbo->base.width, h = cbo->base.height;
    uint32 *fb = win->framebuffer; int fb_w = win->full_width;

    /* Box */
    widget_draw_rect(x, y, w, h, cbo->base.bg_color, fb, fb_w, win->full_height);
    widget_draw_border(x, y, w, h, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);

    /* Selected text */
    if (cbo->selected_idx >= 0 && cbo->selected_idx < cbo->item_count)
        widget_draw_text(x + 4, y + (h - 8) / 2, cbo->items[cbo->selected_idx], WIDGET_TEXT_NORMAL, fb, fb_w, win->full_height);

    /* Dropdown arrow */
    int ax = x + w - 18, ay = y + h / 2;
    for (int i = 0; i < 6; i++) {
        fb[(ay - 2 + i) * fb_w + ax + 6 - i] = WIDGET_TEXT_NORMAL;
        fb[(ay - 2 + i) * fb_w + ax + 6 + i] = WIDGET_TEXT_NORMAL;
    }

    /* Dropdown list */
    if (cbo->dropped) {
        int dh = cbo->item_count * 20; if (dh > cbo->drop_height) dh = cbo->drop_height;
        widget_draw_rect(x, y + h, w, dh, 0xFFFFFFFF, fb, fb_w, win->full_height);
        widget_draw_border(x, y + h, w, dh, WIDGET_BORDER_NORMAL, 1, fb, fb_w, win->full_height);
        for (int i = 0; i < cbo->item_count; i++) {
            widget_draw_text(x + 4, y + h + 2 + i * 20, cbo->items[i], WIDGET_TEXT_NORMAL, fb, fb_w, win->full_height);
        }
    }
}

void cbo_add_item(combobox_widget_t *cbo, const char *text)
{ if (cbo && cbo->item_count < CB_MAX_ITEMS) { strncpy(cbo->items[cbo->item_count], text ? text : "", CB_ITEM_LEN - 1); cbo->item_count++; widget_invalidate(&cbo->base); } }
void cbo_clear(combobox_widget_t *cbo) { if (cbo) { cbo->item_count = 0; cbo->selected_idx = -1; widget_invalidate(&cbo->base); } }
void cbo_select(combobox_widget_t *cbo, int idx) { if (cbo && idx >= 0 && idx < cbo->item_count) { cbo->selected_idx = idx; cbo->dropped = 0; widget_invalidate(&cbo->base); } }
int cbo_get_selected(combobox_widget_t *cbo) { return cbo ? cbo->selected_idx : -1; }
const char *cbo_get_text(combobox_widget_t *cbo) { return (cbo && cbo->selected_idx >= 0) ? cbo->items[cbo->selected_idx] : ""; }

/* ===== Draw all widgets for a window ===== */
void widget_draw_all(window_t *win)
{
    if (!win) return;
    widget_t *w = widget_list;
    while (w) {
        if (w->parent == win && w->visible) {
            switch (w->type) {
            case WIDGET_BUTTON:     btn_draw((button_widget_t *)w); break;
            case WIDGET_LABEL:      lbl_draw((label_widget_t *)w); break;
            case WIDGET_TEXTBOX:    tb_draw((textbox_widget_t *)w); break;
            case WIDGET_SCROLLBAR:  sb_draw((scrollbar_widget_t *)w); break;
            case WIDGET_LISTVIEW:   lv_draw((listview_widget_t *)w); break;
            case WIDGET_MENU:       menu_draw((menu_widget_t *)w); break;
            case WIDGET_DIALOG:     dlg_draw((dialog_widget_t *)w); break;
            case WIDGET_TAB:        tab_draw((tab_widget_t *)w); break;
            case WIDGET_PROGRESSBAR: pb_draw((progressbar_widget_t *)w); break;
            case WIDGET_TOOLTIP:    tt_draw((tooltip_widget_t *)w); break;
            case WIDGET_CHECKBOX:   cbx_draw((checkbox_widget_t *)w); break;
            case WIDGET_SLIDER:     sld_draw((slider_widget_t *)w); break;
            case WIDGET_COMBOBOX:   cbo_draw((combobox_widget_t *)w); break;
            default: break;
            }
        }
        w = w->next;
    }
}

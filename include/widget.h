/*
 * torOS UI Widget Toolkit Header
 * Buttons, TextBox, ScrollBar, ListView, Menu, Dialog, Tab, ProgressBar, Tooltip
 */

#ifndef _WIDGET_H
#define _WIDGET_H

#include "toros.h"
#include "window.h"

/* ===== Widget Types ===== */
typedef enum {
    WIDGET_BUTTON,
    WIDGET_TEXTBOX,
    WIDGET_SCROLLBAR,
    WIDGET_LISTVIEW,
    WIDGET_MENU,
    WIDGET_DIALOG,
    WIDGET_TAB,
    WIDGET_PROGRESSBAR,
    WIDGET_TOOLTIP,
    WIDGET_LABEL,
    WIDGET_CHECKBOX,
    WIDGET_RADIO,
    WIDGET_SLIDER,
    WIDGET_COMBOBOX
} widget_type_t;

/* ===== Widget States ===== */
typedef enum {
    STATE_NORMAL,
    STATE_HOVER,
    STATE_PRESSED,
    STATE_DISABLED,
    STATE_FOCUSED
} widget_state_t;

/* ===== Colors ===== */
#define WIDGET_BG_NORMAL        0xFFF0F0F0
#define WIDGET_BG_HOVER         0xFFE5F1FB
#define WIDGET_BG_PRESSED       0xFFCCE4F7
#define WIDGET_BG_DISABLED      0xFFCCCCCC
#define WIDGET_BORDER_NORMAL    0xFFADADAD
#define WIDGET_BORDER_FOCUSED   0xFF0078D7
#define WIDGET_TEXT_NORMAL      0xFF000000
#define WIDGET_TEXT_DISABLED    0xFF808080
#define WIDGET_ACCENT           0xFF0078D7
#define WIDGET_SELECTION_BG     0xFF0078D7
#define WIDGET_SELECTION_TEXT   0xFFFFFFFF
#define WIDGET_TOOLTIP_BG       0xFFFFFFE1
#define WIDGET_TOOLTIP_BORDER   0xFF000000
#define WIDGET_DIALOG_BG        0xFFF0F0F0
#define WIDGET_DIALOG_TITLE     0xFF0078D7
#define WIDGET_SHADOW           0x80000000

/* ===== Forward Decl ===== */
struct widget;

/* ===== Callbacks ===== */
typedef void (*widget_click_cb)(struct widget *w, int x, int y);
typedef void (*widget_change_cb)(struct widget *w);
typedef int (*widget_key_cb)(struct widget *w, uint16 keycode);

/* ===== Base Widget ===== */
typedef struct widget {
    widget_type_t type;
    widget_state_t state;
    int x, y;
    int width, height;
    uint32 bg_color;
    uint32 fg_color;
    uint32 border_color;
    int visible;
    int enabled;
    int focused;
    char text[256];
    void *user_data;
    window_t *parent;
    struct widget *next;
    widget_click_cb on_click;
    widget_change_cb on_change;
    widget_key_cb on_key;
} widget_t;

/* ===== Button ===== */
typedef struct {
    widget_t base;
    int pressed;
    int toggle_mode;
    int toggled;
    uint32 press_color;
    int border_radius;
} button_widget_t;

/* ===== Label ===== */
typedef struct {
    widget_t base;
    int align;      /* 0=left, 1=center, 2=right */
    int valign;     /* 0=top, 1=center, 2=bottom */
    int font_size;
} label_widget_t;

/* ===== TextBox ===== */
#define TEXTBOX_MAX_LEN     512
#define TEXTBOX_MAX_LINES   32

typedef struct {
    widget_t base;
    char buffer[TEXTBOX_MAX_LEN];
    int cursor_pos;
    int selection_start;
    int selection_end;
    int scroll_x;
    int multiline;
    int password_mode;
    int max_length;
    int read_only;
} textbox_widget_t;

/* ===== ScrollBar ===== */
typedef struct {
    widget_t base;
    int orientation;    /* 0=vertical, 1=horizontal */
    int min_val;
    int max_val;
    int current_val;
    int page_size;
    int thumb_pos;
    int thumb_size;
    int dragging;
    int drag_start;
} scrollbar_widget_t;

/* ===== ListView ===== */
#define LV_MAX_ITEMS        128
#define LV_MAX_COLS         8
#define LV_ITEM_LEN         64

typedef struct {
    char text[LV_ITEM_LEN];
    uint32 bg_color;
    uint32 text_color;
    void *user_data;
} lv_item_t;

typedef struct {
    widget_t base;
    lv_item_t items[LV_MAX_ITEMS];
    int item_count;
    int selected_idx;
    int top_idx;
    int columns;
    int col_widths[LV_MAX_COLS];
    char headers[LV_MAX_COLS][LV_ITEM_LEN];
    int header_height;
    int show_headers;
    scrollbar_widget_t *vscroll;
    scrollbar_widget_t *hscroll;
} listview_widget_t;

/* ===== Menu ===== */
#define MENU_MAX_ITEMS      32
#define MENU_ITEM_LEN       48

typedef struct {
    char text[MENU_ITEM_LEN];
    uint32 shortcut_key;
    int enabled;
    int separator;
    widget_click_cb on_click;
} menu_item_t;

typedef struct {
    widget_t base;
    menu_item_t items[MENU_MAX_ITEMS];
    int item_count;
    int selected_idx;
    int open;
    int item_height;
} menu_widget_t;

/* ===== Dialog ===== */
#define DLG_MAX_BUTTONS     4
#define DLG_TITLE_LEN       128
#define DLG_MSG_LEN         512

typedef struct {
    widget_t base;
    char title[DLG_TITLE_LEN];
    char message[DLG_MSG_LEN];
    button_widget_t buttons[DLG_MAX_BUTTONS];
    int button_count;
    int result;
    int modal;
    widget_t *parent_widget;
} dialog_widget_t;

/* ===== Tab Control ===== */
#define TAB_MAX_TABS        16
#define TAB_LABEL_LEN       32

typedef struct {
    char label[TAB_LABEL_LEN];
    widget_t *content;
    int active;
} tab_page_t;

typedef struct {
    widget_t base;
    tab_page_t tabs[TAB_MAX_TABS];
    int tab_count;
    int active_tab;
    int tab_height;
} tab_widget_t;

/* ===== ProgressBar ===== */
typedef struct {
    widget_t base;
    int min_val;
    int max_val;
    int current_val;
    int show_percent;
    uint32 bar_color;
    int orientation;
} progressbar_widget_t;

/* ===== Tooltip ===== */
typedef struct {
    widget_t base;
    char tooltip_text[256];
    int auto_hide_ms;
    uint64 show_time;
    int follow_mouse;
} tooltip_widget_t;

/* ===== Checkbox ===== */
typedef struct {
    widget_t base;
    int checked;
    int box_size;
} checkbox_widget_t;

/* ===== Radio ===== */
typedef struct {
    widget_t base;
    int selected;
    int circle_size;
    struct radio_widget *group_next;
} radio_widget_t;

/* ===== Slider ===== */
typedef struct {
    widget_t base;
    int min_val;
    int max_val;
    int current_val;
    int orientation;
    int thumb_size;
    int dragging;
} slider_widget_t;

/* ===== ComboBox ===== */
#define CB_MAX_ITEMS        32
#define CB_ITEM_LEN         48

typedef struct {
    widget_t base;
    char items[CB_MAX_ITEMS][CB_ITEM_LEN];
    int item_count;
    int selected_idx;
    int dropped;
    int drop_height;
} combobox_widget_t;

/* ===== Widget Core API ===== */
void widget_init(void);
void widget_draw_all(window_t *win);
int widget_handle_mouse(window_t *win, int x, int y, int button, int pressed);
int widget_handle_key(window_t *win, uint16 keycode);
widget_t *widget_at_pos(window_t *win, int x, int y);
void widget_set_focus(widget_t *w);
void widget_invalidate(widget_t *w);

/* ===== Button API ===== */
button_widget_t *btn_create(window_t *parent, int x, int y, int w, int h, const char *text);
void btn_draw(button_widget_t *btn);
void btn_set_text(button_widget_t *btn, const char *text);
void btn_set_toggle(button_widget_t *btn, int toggle);
int btn_get_toggle(button_widget_t *btn);
void btn_set_colors(button_widget_t *btn, uint32 normal, uint32 hover, uint32 pressed);

/* ===== Label API ===== */
label_widget_t *lbl_create(window_t *parent, int x, int y, int w, int h, const char *text);
void lbl_draw(label_widget_t *lbl);
void lbl_set_text(label_widget_t *lbl, const char *text);
void lbl_set_align(label_widget_t *lbl, int halign, int valign);

/* ===== TextBox API ===== */
textbox_widget_t *tb_create(window_t *parent, int x, int y, int w, int h);
void tb_draw(textbox_widget_t *tb);
void tb_set_text(textbox_widget_t *tb, const char *text);
const char *tb_get_text(textbox_widget_t *tb);
void tb_insert(textbox_widget_t *tb, const char *text);
void tb_delete_selection(textbox_widget_t *tb);
void tb_set_cursor(textbox_widget_t *tb, int pos);
void tb_set_password(textbox_widget_t *tb, int password);
void tb_set_multiline(textbox_widget_t *tb, int multiline);
void tb_set_readonly(textbox_widget_t *tb, int ro);
void tb_handle_key(textbox_widget_t *tb, uint16 keycode);

/* ===== ScrollBar API ===== */
scrollbar_widget_t *sb_create(window_t *parent, int x, int y, int w, int h, int vertical);
void sb_draw(scrollbar_widget_t *sb);
void sb_set_range(scrollbar_widget_t *sb, int min, int max, int page);
void sb_set_value(scrollbar_widget_t *sb, int val);
int sb_get_value(scrollbar_widget_t *sb);

/* ===== ListView API ===== */
listview_widget_t *lv_create(window_t *parent, int x, int y, int w, int h);
void lv_draw(listview_widget_t *lv);
void lv_add_item(listview_widget_t *lv, const char *text);
void lv_clear(listview_widget_t *lv);
void lv_set_columns(listview_widget_t *lv, int cols, int widths[]);
void lv_set_headers(listview_widget_t *lv, char *headers[]);
int lv_get_selected(listview_widget_t *lv);
const char *lv_get_item(listview_widget_t *lv, int idx);

/* ===== Menu API ===== */
menu_widget_t *menu_create(window_t *parent, int x, int y, int w, int h);
void menu_draw(menu_widget_t *menu);
void menu_add_item(menu_widget_t *menu, const char *text, widget_click_cb cb);
void menu_add_separator(menu_widget_t *menu);
void menu_show(menu_widget_t *menu, int x, int y);
void menu_hide(menu_widget_t *menu);

/* ===== Dialog API ===== */
dialog_widget_t *dlg_create(const char *title, const char *message, int modal);
void dlg_draw(dialog_widget_t *dlg);
void dlg_add_button(dialog_widget_t *dlg, const char *text, int result);
int dlg_show_modal(dialog_widget_t *dlg);
void dlg_close(dialog_widget_t *dlg, int result);

/* ===== Tab API ===== */
tab_widget_t *tab_create(window_t *parent, int x, int y, int w, int h);
void tab_draw(tab_widget_t *tab);
int tab_add(tab_widget_t *tab, const char *label);
void tab_set_active(tab_widget_t *tab, int idx);
int tab_get_active(tab_widget_t *tab);

/* ===== ProgressBar API ===== */
progressbar_widget_t *pb_create(window_t *parent, int x, int y, int w, int h);
void pb_draw(progressbar_widget_t *pb);
void pb_set_range(progressbar_widget_t *pb, int min, int max);
void pb_set_value(progressbar_widget_t *pb, int val);
int pb_get_value(progressbar_widget_t *pb);

/* ===== Tooltip API ===== */
tooltip_widget_t *tt_create(window_t *parent);
void tt_draw(tooltip_widget_t *tt);
void tt_show(tooltip_widget_t *tt, const char *text, int x, int y);
void tt_hide(tooltip_widget_t *tt);
void tt_set_delay(tooltip_widget_t *tt, int ms);

/* ===== Checkbox API ===== */
checkbox_widget_t *cbx_create(window_t *parent, int x, int y, int w, int h, const char *text);
void cbx_draw(checkbox_widget_t *cbx);
void cbx_set_checked(checkbox_widget_t *cbx, int checked);
int cbx_get_checked(checkbox_widget_t *cbx);

/* ===== Slider API ===== */
slider_widget_t *sld_create(window_t *parent, int x, int y, int w, int h, int vertical);
void sld_draw(slider_widget_t *sld);
void sld_set_range(slider_widget_t *sld, int min, int max);
void sld_set_value(slider_widget_t *sld, int val);
int sld_get_value(slider_widget_t *sld);

/* ===== ComboBox API ===== */
combobox_widget_t *cbo_create(window_t *parent, int x, int y, int w, int h);
void cbo_draw(combobox_widget_t *cbo);
void cbo_add_item(combobox_widget_t *cbo, const char *text);
void cbo_clear(combobox_widget_t *cbo);
void cbo_select(combobox_widget_t *cbo, int idx);
int cbo_get_selected(combobox_widget_t *cbo);
const char *cbo_get_text(combobox_widget_t *cbo);

/* ===== Draw Helpers ===== */
void widget_draw_rect(int x, int y, int w, int h, uint32 color, uint32 *fb, int fb_w, int fb_h);
void widget_draw_border(int x, int y, int w, int h, uint32 color, int width, uint32 *fb, int fb_w, int fb_h);
void widget_draw_text(int x, int y, const char *text, uint32 color, uint32 *fb, int fb_w, int fb_h);
void widget_draw_gradient(int x, int y, int w, int h, uint32 top, uint32 bottom, uint32 *fb, int fb_w, int fb_h);
void widget_draw_rounded_rect(int x, int y, int w, int h, int r, uint32 color, uint32 *fb, int fb_w, int fb_h);

#endif

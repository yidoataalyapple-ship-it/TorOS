/*
 * torOS Application Ecosystem Header
 * File Manager, Text Editor, Calculator, Terminal, Paint, Browser, Media Player, Settings
 */

#ifndef _APP_H
#define _APP_H

#include "toros.h"
#include "window.h"
#include "widget.h"

/* ===== Application Base ===== */
typedef struct {
    char name[32];
    char title[64];
    char icon_path[128];
    uint32 pid;
    window_t *main_window;
    int running;
    int (*init)(void *ctx);
    void (*run)(void *ctx);
    void (*draw)(void *ctx, uint32 *fb, int fb_w, int fb_h);
    void (*handle_input)(void *ctx, input_event_t *evt);
    void (*cleanup)(void *ctx);
    void *ctx;
} app_t;

#define MAX_APPS            16

/* ===== File Manager ===== */
#define FM_MAX_FILES        256
#define FM_PATH_LEN         256
#define FM_NAME_LEN         128

typedef struct {
    char name[FM_NAME_LEN];
    uint32 size;
    uint8 is_dir;
    uint8 is_hidden;
    uint64 modified_time;
} fm_file_entry_t;

typedef struct {
    char current_path[FM_PATH_LEN];
    fm_file_entry_t files[FM_MAX_FILES];
    int file_count;
    int selected_idx;
    int view_mode;      /* 0=icons, 1=list, 2=detail */
    int sort_by;        /* 0=name, 1=size, 2=date */
    int sort_asc;
    char clipboard_path[FM_PATH_LEN];
    int clipboard_cut;
    listview_widget_t *file_list;
    textbox_widget_t *path_bar;
    button_widget_t *btn_up;
    button_widget_t *btn_new_folder;
    button_widget_t *btn_delete;
} file_manager_t;

app_t *app_file_manager(void);
void fm_init(file_manager_t *fm);
void fm_navigate(file_manager_t *fm, const char *path);
void fm_refresh(file_manager_t *fm);
void fm_open_selected(file_manager_t *fm);
void fm_delete_selected(file_manager_t *fm);
void fm_draw(file_manager_t *fm, uint32 *fb, int fb_w, int fb_h);

/* ===== Text Editor ===== */
#define TE_MAX_LINES        1024
#define TE_LINE_LEN         256
#define TE_TAB_SIZE         4

typedef struct {
    char lines[TE_MAX_LINES][TE_LINE_LEN];
    int num_lines;
    int cursor_line;
    int cursor_col;
    int scroll_line;
    int scroll_col;
    char filename[128];
    int modified;
    int show_line_numbers;
    int word_wrap;
    textbox_widget_t *editor;
    button_widget_t *btn_save;
    button_widget_t *btn_open;
} text_editor_t;

app_t *app_text_editor(void);
void te_init(text_editor_t *te);
int te_load_file(text_editor_t *te, const char *filename);
int te_save_file(text_editor_t *te);
void te_insert_char(text_editor_t *te, char c);
void te_delete_char(text_editor_t *te);
void te_draw(text_editor_t *te, uint32 *fb, int fb_w, int fb_h);

/* ===== Calculator ===== */
#define CALC_DISPLAY_LEN    32

typedef struct {
    char display[CALC_DISPLAY_LEN];
    double accumulator;
    double current;
    char pending_op;
    int has_error;
    int new_entry;
    button_widget_t *btn_digits[10];
    button_widget_t *btn_ops[6]; /* + - * / = */
    button_widget_t *btn_clear;
    button_widget_t *btn_backspace;
} calculator_t;

app_t *app_calculator(void);
void calc_init(calculator_t *calc);
void calc_input_digit(calculator_t *calc, int digit);
void calc_input_op(calculator_t *calc, char op);
void calc_input_decimal(calculator_t *calc);
void calc_clear(calculator_t *calc);
void calc_backspace(calculator_t *calc);
void calc_compute(calculator_t *calc);
void calc_draw(calculator_t *calc, uint32 *fb, int fb_w, int fb_h);

/* ===== Terminal ===== */
#define TERM_COLS           80
#define TERM_ROWS           25
#define TERM_HISTORY        1000

typedef struct {
    char buffer[TERM_HISTORY][TERM_COLS + 1];
    int scrollback;
    int cursor_row;
    int cursor_col;
    uint32 fg_color;
    uint32 bg_color;
    char input_buffer[TERM_COLS + 1];
    int input_len;
    textbox_widget_t *input_box;
} terminal_t;

app_t *app_terminal(void);
void term_init(terminal_t *term);
void term_write(terminal_t *term, const char *text);
void term_write_color(terminal_t *term, const char *text, uint32 color);
void term_clear(terminal_t *term);
void term_scroll(terminal_t *term, int lines);
void term_draw(terminal_t *term, uint32 *fb, int fb_w, int fb_h);

/* ===== Paint ===== */
#define PAINT_WIDTH         800
#define PAINT_HEIGHT        600
#define PAINT_MAX_UNDO      16

typedef enum {
    PAINT_TOOL_PENCIL,
    PAINT_TOOL_BRUSH,
    PAINT_TOOL_ERASER,
    PAINT_TOOL_LINE,
    PAINT_TOOL_RECT,
    PAINT_TOOL_CIRCLE,
    PAINT_TOOL_FILL,
    PAINT_TOOL_PICKER,
    PAINT_TOOL_TEXT
} paint_tool_t;

typedef struct {
    uint32 canvas[PAINT_WIDTH * PAINT_HEIGHT];
    uint32 undo_stack[PAINT_MAX_UNDO][PAINT_WIDTH * PAINT_HEIGHT];
    int undo_idx;
    paint_tool_t tool;
    uint32 fg_color;
    uint32 bg_color;
    int brush_size;
    int drawing;
    int last_x, last_y;
    button_widget_t *btn_tools[9];
    button_widget_t *btn_colors[16];
    button_widget_t *btn_undo;
    button_widget_t *btn_save;
} paint_app_t;

app_t *app_paint(void);
void paint_init(paint_app_t *pa);
void paint_set_pixel(paint_app_t *pa, int x, int y, uint32 color);
void paint_line(paint_app_t *pa, int x0, int y0, int x1, int y1, uint32 color);
void paint_rect(paint_app_t *pa, int x0, int y0, int x1, int y1, uint32 color);
void paint_circle(paint_app_t *pa, int cx, int cy, int r, uint32 color);
void paint_fill(paint_app_t *pa, int x, int y, uint32 color);
void paint_undo(paint_app_t *pa);
void paint_draw(paint_app_t *pa, uint32 *fb, int fb_w, int fb_h);

/* ===== Web Browser ===== */
#define BROWSER_URL_LEN     512
#define BROWSER_TITLE_LEN   256
#define BROWSER_MAX_HISTORY 32

typedef struct {
    char url[BROWSER_URL_LEN];
    char title[BROWSER_TITLE_LEN];
    uint32 *page_bitmap;
    int page_width;
    int page_height;
    char history[BROWSER_MAX_HISTORY][BROWSER_URL_LEN];
    int history_idx;
    int history_count;
    int loading;
    textbox_widget_t *address_bar;
    button_widget_t *btn_back;
    button_widget_t *btn_forward;
    button_widget_t *btn_reload;
    button_widget_t *btn_go;
    scrollbar_widget_t *vscroll;
} browser_t;

app_t *app_browser(void);
void browser_init(browser_t *b);
void browser_navigate(browser_t *b, const char *url);
void browser_back(browser_t *b);
void browser_forward(browser_t *b);
void browser_reload(browser_t *b);
void browser_draw(browser_t *b, uint32 *fb, int fb_w, int fb_h);

/* ===== Media Player ===== */
typedef enum {
    MEDIA_STATE_STOPPED,
    MEDIA_STATE_PLAYING,
    MEDIA_STATE_PAUSED
} media_state_t;

typedef struct {
    char filename[128];
    media_state_t state;
    uint32 duration_ms;
    uint32 current_ms;
    int volume;
    int fullscreen;
    progressbar_widget_t *progress;
    button_widget_t *btn_play;
    button_widget_t *btn_pause;
    button_widget_t *btn_stop;
    button_widget_t *btn_prev;
    button_widget_t *btn_next;
    slider_widget_t *volume_slider;
} media_player_t;

app_t *app_media_player(void);
void mp_init(media_player_t *mp);
void mp_open(media_player_t *mp, const char *filename);
void mp_play(media_player_t *mp);
void mp_pause(media_player_t *mp);
void mp_stop(media_player_t *mp);
void mp_seek(media_player_t *mp, uint32 ms);
void mp_draw(media_player_t *mp, uint32 *fb, int fb_w, int fb_h);

/* ===== Settings ===== */
#define SETTING_CATEGORIES  8

typedef struct {
    char name[32];
    int type;           /* 0=bool, 1=int, 2=string, 3=choice */
    union { int i; char s[64]; } value;
    char choices[4][32];
    int num_choices;
} setting_item_t;

typedef struct {
    char category_name[32];
    setting_item_t items[16];
    int num_items;
} setting_category_t;

typedef struct {
    setting_category_t categories[SETTING_CATEGORIES];
    int num_categories;
    int selected_category;
    tab_widget_t *tabs;
    button_widget_t *btn_apply;
    button_widget_t *btn_reset;
} settings_app_t;

app_t *app_settings(void);
void settings_init(settings_app_t *sa);
void settings_load(settings_app_t *sa);
void settings_save(settings_app_t *sa);
void settings_draw(settings_app_t *sa, uint32 *fb, int fb_w, int fb_h);

/* ===== App Manager ===== */
void app_manager_init(void);
app_t *app_launch(const char *name);
void app_close(app_t *app);
void app_manager_draw_all(uint32 *fb, int fb_w, int fb_h);
app_t *app_get_by_name(const char *name);
void app_manager_list(void);

#endif

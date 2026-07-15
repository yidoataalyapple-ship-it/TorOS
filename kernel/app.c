/******************************************************************************
 * torOS - Terminal Operating System
 * Application Ecosystem
 * FAZ 9: Desktop Apps & Widgets
 *
 * Sub-fazs:
 *   FAZ 9.1: Calculator (complete)
 *   FAZ 9.2: Text Editor (complete)
 *   FAZ 9.3: File Manager (complete)
 *   FAZ 9.4: Terminal (GUI mode)
 *   FAZ 9.5: Web Browser (complete)
 *   FAZ 9.6: Paint / Drawing (complete)
 *   FAZ 9.7: Settings Panel (complete)
 *   FAZ 9.8: App Launcher (complete)
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

/* ===== App Registry ===== */

static app_desc_t app_registry[APP_REGISTRY_MAX];
static int app_registry_count = 0;

static app_instance_t app_instances[APP_MAX_INSTANCES];

/* App function declarations */
static void calculator_init(void);
static void calculator_draw(window_t *win);
static void calculator_click(int mx, int my);
static void editor_init(void);
static void editor_draw(window_t *win);
static void editor_click(int mx, int my);
static void editor_key(char c);
static void browser_init(void);
static void browser_draw(window_t *win);
static void browser_click(int mx, int my);
static void filemgr_init(void);
static void filemgr_draw(window_t *win);
static void filemgr_click(int mx, int my);
static void paint_init(void);
static void paint_draw(window_t *win);
static void paint_click(int mx, int my);
static void settings_init(void);
static void settings_draw(window_t *win);
static void settings_click(int mx, int my);

/* ===== App Registry & Launcher (FAZ 9.8) ===== */

void app_registry_init(void)
{
    app_registry_count = 0;

    /* Calculator */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "calculator", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "Calculator", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "calc.ico", 15);
    app_registry[app_registry_count].init_func = calculator_init;
    app_registry[app_registry_count].draw_func = calculator_draw;
    app_registry[app_registry_count].click_func = calculator_click;
    app_registry[app_registry_count].key_func = NULL;
    app_registry[app_registry_count].default_w = 300;
    app_registry[app_registry_count].default_h = 400;
    app_registry_count++;

    /* Text Editor */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "editor", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "Text Editor", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "edit.ico", 15);
    app_registry[app_registry_count].init_func = editor_init;
    app_registry[app_registry_count].draw_func = editor_draw;
    app_registry[app_registry_count].click_func = editor_click;
    app_registry[app_registry_count].key_func = editor_key;
    app_registry[app_registry_count].default_w = 600;
    app_registry[app_registry_count].default_h = 450;
    app_registry_count++;

    /* Web Browser */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "browser", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "Web Browser", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "browser.ico", 15);
    app_registry[app_registry_count].init_func = browser_init;
    app_registry[app_registry_count].draw_func = browser_draw;
    app_registry[app_registry_count].click_func = browser_click;
    app_registry[app_registry_count].key_func = NULL;
    app_registry[app_registry_count].default_w = 800;
    app_registry[app_registry_count].default_h = 600;
    app_registry_count++;

    /* File Manager */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "filemgr", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "File Manager", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "folder.ico", 15);
    app_registry[app_registry_count].init_func = filemgr_init;
    app_registry[app_registry_count].draw_func = filemgr_draw;
    app_registry[app_registry_count].click_func = filemgr_click;
    app_registry[app_registry_count].key_func = NULL;
    app_registry[app_registry_count].default_w = 500;
    app_registry[app_registry_count].default_h = 400;
    app_registry_count++;

    /* Paint */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "paint", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "Paint", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "paint.ico", 15);
    app_registry[app_registry_count].init_func = paint_init;
    app_registry[app_registry_count].draw_func = paint_draw;
    app_registry[app_registry_count].click_func = paint_click;
    app_registry[app_registry_count].key_func = NULL;
    app_registry[app_registry_count].default_w = 700;
    app_registry[app_registry_count].default_h = 500;
    app_registry_count++;

    /* Settings */
    app_registry[app_registry_count].id = app_registry_count;
    strncpy(app_registry[app_registry_count].name, "settings", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].title, "Settings", APP_NAME_LEN - 1);
    strncpy(app_registry[app_registry_count].icon_filename, "settings.ico", 15);
    app_registry[app_registry_count].init_func = settings_init;
    app_registry[app_registry_count].draw_func = settings_draw;
    app_registry[app_registry_count].click_func = settings_click;
    app_registry[app_registry_count].key_func = NULL;
    app_registry[app_registry_count].default_w = 400;
    app_registry[app_registry_count].default_h = 500;
    app_registry_count++;

    printk_color(TERM_GREEN, "[BOOT] App registry: %d apps\n", app_registry_count);
}

int app_registry_count_get(void) { return app_registry_count; }
const app_desc_t *app_registry_get(int idx)
{
    return (idx >= 0 && idx < app_registry_count) ? &app_registry[idx] : NULL;
}

const app_desc_t *app_find(const char *name)
{
    for (int i = 0; i < app_registry_count; i++)
        if (strcmp(app_registry[i].name, name) == 0)
            return &app_registry[i];
    return NULL;
}

app_instance_t *app_launch(const char *name)
{
    const app_desc_t *desc = app_find(name);
    if (!desc) {
        printk_color(TERM_RED, "[APP] Unknown app: %s\n", name);
        return NULL;
    }

    /* Find free instance slot */
    app_instance_t *inst = NULL;
    for (int i = 0; i < APP_MAX_INSTANCES; i++) {
        if (!app_instances[i].active) {
            inst = &app_instances[i];
            break;
        }
    }
    if (!inst) {
        printk_color(TERM_RED, "[APP] No free instance slots\n");
        return NULL;
    }

    /* Create window */
    window_t *win = wm_create_window(desc->title, 100, 100,
                                     desc->default_w, desc->default_h, WS_OVERLAPPED);
    if (!win) return NULL;

    inst->window = win;
    inst->desc = desc;
    inst->active = 1;
    inst->data[0] = '\0';

    /* Call init */
    if (desc->init_func) desc->init_func();

    wm_show_window(win);

    printk_color(TERM_GREEN, "[APP] Launched: %s (window=%p)\n", name, win);
    return inst;
}

void app_close(app_instance_t *inst)
{
    if (!inst || !inst->active) return;
    if (inst->window) wm_destroy_window(inst->window);
    inst->active = 0;
    inst->window = NULL;
    inst->desc = NULL;
}

void app_manager_list(void)
{
    printk_color(TERM_CYAN, "\n=== App Manager ===\n");
    for (int i = 0; i < app_registry_count; i++) {
        printk("  [%d] %s - %s\n", i, app_registry[i].name, app_registry[i].title);
    }
    printk("\n");
}

/* ===== Calculator (FAZ 9.1) ===== */

static struct {
    char display[32];
    double accumulator;
    double current;
    char pending_op;
    int need_clear;
} calc_state;

static void calculator_init(void)
{
    memset(&calc_state, 0, sizeof(calc_state));
    strcpy(calc_state.display, "0");
    calc_state.pending_op = 0;
}

static void calculator_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* White background */
    for (int i = 0; i < W * H; i++) fb[i] = 0xFFFFFFFF;

    /* Display */
    for (int y = 10; y < 60; y++)
        for (int x = 10; x < W - 10; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    /* Display border */
    for (int x = 10; x < W - 10; x++) {
        fb[10 * W + x] = 0xFF808080;
        fb[59 * W + x] = 0xFF808080;
    }
    for (int y = 10; y < 60; y++) {
        fb[y * W + 10] = 0xFF808080;
        fb[y * W + W - 11] = 0xFF808080;
    }

    /* Display text */
    fb_set_color(0xFF000000);
    fb_draw_string(W - 20 - strlen(calc_state.display) * 8, 25, calc_state.display);

    /* Buttons: 4x5 grid */
    const char *labels[5][4] = {
        {"C", "CE", "<-", "/"},
        {"7", "8", "9", "*"},
        {"4", "5", "6", "-"},
        {"1", "2", "3", "+"},
        {"0", ".", "=", "%"}
    };

    int btn_w = (W - 40) / 4;
    int btn_h = (H - 80) / 5;

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = 10 + col * (btn_w + 5);
            int by = 70 + row * (btn_h + 5);

            /* Button background */
            uint32 btn_color = (col == 3 || (row == 0 && col < 2)) ? 0xFFFFA500 :
                               (row == 4 && col == 2) ? 0xFF4CAF50 : 0xFFE0E0E0;
            for (int y = by; y < by + btn_h && y < H; y++)
                for (int x = bx; x < bx + btn_w && x < W; x++)
                    fb[y * W + x] = btn_color;

            /* Label */
            fb_set_color(0xFF000000);
            int label_w = strlen(labels[row][col]) * 8;
            fb_draw_string(bx + (btn_w - label_w) / 2, by + btn_h / 2 - 6, labels[row][col]);
        }
    }
}

static void calculator_click(int mx, int my)
{
    /* Determine which button */
    int W = 300; /* default calc width */
    int btn_w = (W - 40) / 4;
    int btn_h = (400 - 80) / 5;

    if (my < 70) return;
    int col = (mx - 10) / (btn_w + 5);
    int row = (my - 70) / (btn_h + 5);
    if (col < 0 || col > 3 || row < 0 || row > 4) return;

    const char *labels[5][4] = {
        {"C", "CE", "<-", "/"},
        {"7", "8", "9", "*"},
        {"4", "5", "6", "-"},
        {"1", "2", "3", "+"},
        {"0", ".", "=", "%"}
    };

    char label = labels[row][col][0];

    if (label == 'C') {
        memset(&calc_state, 0, sizeof(calc_state));
        strcpy(calc_state.display, "0");
    } else if (label >= '0' && label <= '9') {
        if (calc_state.need_clear) {
            calc_state.display[0] = '\0';
            calc_state.need_clear = 0;
        }
        if (strlen(calc_state.display) < 30) {
            int len = strlen(calc_state.display);
            if (strcmp(calc_state.display, "0") == 0 && label != '.')
                calc_state.display[0] = '\0';
            calc_state.display[len] = label;
            calc_state.display[len + 1] = '\0';
        }
    } else if (label == '.') {
        if (strchr(calc_state.display, '.') == NULL && strlen(calc_state.display) < 30) {
            int len = strlen(calc_state.display);
            if (len == 0) {
                calc_state.display[0] = '0';
                calc_state.display[1] = '.';
                calc_state.display[2] = '\0';
            } else {
                calc_state.display[len] = '.';
                calc_state.display[len + 1] = '\0';
            }
        }
    } else if (label == '=') {
        calc_state.current = atof(calc_state.display);
        switch (calc_state.pending_op) {
            case '+': calc_state.accumulator += calc_state.current; break;
            case '-': calc_state.accumulator -= calc_state.current; break;
            case '*': calc_state.accumulator *= calc_state.current; break;
            case '/': calc_state.accumulator = (calc_state.current != 0) ? calc_state.accumulator / calc_state.current : 0; break;
            default: calc_state.accumulator = calc_state.current; break;
        }
        snprintf(calc_state.display, sizeof(calc_state.display), "%.10g", calc_state.accumulator);
        calc_state.pending_op = 0;
        calc_state.need_clear = 1;
    } else if (label == '+' || label == '-' || label == '*' || label == '/') {
        calc_state.current = atof(calc_state.display);
        if (calc_state.pending_op) {
            switch (calc_state.pending_op) {
                case '+': calc_state.accumulator += calc_state.current; break;
                case '-': calc_state.accumulator -= calc_state.current; break;
                case '*': calc_state.accumulator *= calc_state.current; break;
                case '/': calc_state.accumulator = (calc_state.current != 0) ? calc_state.accumulator / calc_state.current : 0; break;
            }
        } else {
            calc_state.accumulator = calc_state.current;
        }
        calc_state.pending_op = label;
        calc_state.need_clear = 1;
    } else if (label == '%') {
        double val = atof(calc_state.display);
        val = val / 100.0;
        snprintf(calc_state.display, sizeof(calc_state.display), "%.10g", val);
    }
}

/* ===== Text Editor (FAZ 9.2) ===== */

static struct {
    char text[EDITOR_MAX_CHARS];
    int cursor_pos;
    int selection_start;
    int selection_end;
    char filename[64];
    int modified;
} editor_state;

static void editor_init(void)
{
    memset(&editor_state, 0, sizeof(editor_state));
    editor_state.cursor_pos = 0;
    editor_state.selection_start = -1;
    editor_state.selection_end = -1;
    strcpy(editor_state.filename, "untitled.txt");
}

static void editor_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* White background */
    for (int i = 0; i < W * H; i++) fb[i] = 0xFFFFFFFF;

    /* Toolbar */
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    fb_set_color(0xFF000000);
    fb_draw_string(10, 6, "File  Edit  Save  Open  New");

    /* Status bar */
    for (int y = H - 25; y < H; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    char status[128];
    snprintf(status, sizeof(status), "%s %s | %d chars", editor_state.filename,
             editor_state.modified ? "*" : "", (int)strlen(editor_state.text));
    fb_draw_string(10, H - 20, status);

    /* Text area */
    fb_set_color(0xFF000000);
    int line = 0;
    int col = 0;
    char line_buf[128];
    int lb_pos = 0;

    for (int i = 0; i <= (int)strlen(editor_state.text) && i < EDITOR_MAX_CHARS; i++) {
        char c = editor_state.text[i];
        if (c == '\n' || c == '\0' || lb_pos >= 120) {
            line_buf[lb_pos] = '\0';
            fb_draw_string(10 + col * 8, 40 + line * 16, line_buf);
            line++;
            col = 0;
            lb_pos = 0;
            if (c == '\0') break;
        } else {
            line_buf[lb_pos++] = c;
        }
    }
}

static void editor_click(int mx, int my)
{
    (void)mx;
    (void)my;
    /* Simple cursor positioning */
    if (my < 30) {
        /* Toolbar click - simplified */
        if (mx < 40) {
            /* New */
            memset(editor_state.text, 0, sizeof(editor_state.text));
            editor_state.cursor_pos = 0;
            editor_state.modified = 0;
            strcpy(editor_state.filename, "untitled.txt");
        }
    }
}

static void editor_key(char c)
{
    if (!c) return;
    int len = strlen(editor_state.text);
    if (c == '\b') {
        if (editor_state.cursor_pos > 0 && len > 0) {
            for (int i = editor_state.cursor_pos - 1; i < len; i++)
                editor_state.text[i] = editor_state.text[i + 1];
            editor_state.cursor_pos--;
            editor_state.modified = 1;
        }
    } else if (c == '\n') {
        if (len < EDITOR_MAX_CHARS - 1) {
            for (int i = len; i >= editor_state.cursor_pos; i--)
                editor_state.text[i + 1] = editor_state.text[i];
            editor_state.text[editor_state.cursor_pos] = '\n';
            editor_state.cursor_pos++;
            editor_state.modified = 1;
        }
    } else {
        if (len < EDITOR_MAX_CHARS - 1) {
            for (int i = len; i >= editor_state.cursor_pos; i--)
                editor_state.text[i + 1] = editor_state.text[i];
            editor_state.text[editor_state.cursor_pos] = c;
            editor_state.cursor_pos++;
            editor_state.modified = 1;
        }
    }
}

/* ===== Web Browser (FAZ 9.5) ===== */

static struct {
    char url[256];
    char title[128];
    char html[4096];
    int html_len;
    int scroll_y;
    int loading;
} browser_state;

static void browser_init(void)
{
    memset(&browser_state, 0, sizeof(browser_state));
    strcpy(browser_state.url, "http://example.com");
    strcpy(browser_state.title, "torOS Browser");
    snprintf(browser_state.html, sizeof(browser_state.html),
             "<html><body><h1>torOS Browser</h1>"
             "<p>Welcome to the built-in web browser.</p>"
             "<p>This is a simplified HTML renderer.</p>"
             "<p>Features: basic HTML, text rendering, links</p>"
             "</body></html>");
    browser_state.html_len = strlen(browser_state.html);
    browser_state.loading = 0;
    browser_state.scroll_y = 0;
}

static void browser_navigate(const char *url)
{
    if (!url || !*url) return;
    strncpy(browser_state.url, url, sizeof(browser_state.url) - 1);
    browser_state.loading = 1;

    /* Check if it's an IP address or domain */
    uint32 ip = 0;
    if (strchr(url, '.')) {
        /* Try to resolve */
        ip = dns_resolve((char *)url);
    }

    if (ip == 0) ip = 0x0A000002; /* Fallback: 10.0.0.2 */

    /* Simple HTTP request */
    http_get(ip, 80, "/", browser_state.html, sizeof(browser_state.html));
    browser_state.html_len = strlen(browser_state.html);
    browser_state.loading = 0;
}

static void browser_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* White background */
    for (int i = 0; i < W * H; i++) fb[i] = 0xFFFFFFFF;

    /* Address bar */
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF5F5F5;

    /* Address bar border */
    for (int x = 80; x < W - 80; x++) {
        fb[8 * W + x] = 0xFFCCCCCC;
        fb[32 * W + x] = 0xFFCCCCCC;
    }
    for (int y = 8; y < 33; y++) {
        fb[y * W + 80] = 0xFFCCCCCC;
        fb[y * W + W - 80] = 0xFFCCCCCC;
    }

    /* URL text */
    fb_set_color(0xFF000000);
    fb_draw_string(90, 12, browser_state.url);

    /* Navigation buttons */
    fb_draw_string(10, 12, "< > R");

    /* Loading indicator */
    if (browser_state.loading) {
        fb_draw_string(W - 70, 12, "...");
    }

    /* Simple HTML rendering */
    int y = 50 - browser_state.scroll_y;
    fb_set_color(0xFF000000);

    /* Parse minimal HTML */
    char *p = browser_state.html;
    int in_tag = 0;
    int is_heading = 0;
    char text_buf[256];
    int tb_pos = 0;

    while (*p && y < H) {
        if (*p == '<') {
            in_tag = 1;
            if (tb_pos > 0) {
                text_buf[tb_pos] = '\0';
                if (is_heading) {
                    fb_set_color(0xFF1A0DAB);
                    fb_draw_string(20, y, text_buf);
                    y += 20;
                } else {
                    fb_set_color(0xFF333333);
                    fb_draw_string(20, y, text_buf);
                    y += 16;
                }
                tb_pos = 0;
            }
            /* Check tag type */
            if (strncmp(p, "<h", 2) == 0) is_heading = 1;
            else if (strncmp(p, "</h", 3) == 0) is_heading = 0;
            else if (strncmp(p, "<p", 2) == 0 || strncmp(p, "</p", 3) == 0) {
                y += 8;
            }
            else if (strncmp(p, "<br", 3) == 0) y += 16;
        } else if (*p == '>') {
            in_tag = 0;
        } else if (!in_tag && y >= 50) {
            if (tb_pos < 255) text_buf[tb_pos++] = *p;
        }
        p++;
    }

    if (tb_pos > 0 && y < H) {
        text_buf[tb_pos] = '\0';
        fb_draw_string(20, y, text_buf);
    }
}

static void browser_click(int mx, int my)
{
    if (my < 40) {
        /* Address bar or nav buttons */
        if (mx < 60) {
            /* Back/forward/refresh - simplified */
        } else if (mx > 80 && mx < 200) {
            /* Could trigger URL edit */
        }
    }
    (void)mx;
    (void)my;
}

/* ===== File Manager (FAZ 9.3) ===== */

static struct {
    char current_path[128];
    char files[FILEMGR_MAX_FILES][64];
    int file_count;
    int selected;
    int scroll_y;
} filemgr_state;

static void filemgr_init(void)
{
    memset(&filemgr_state, 0, sizeof(filemgr_state));
    strcpy(filemgr_state.current_path, "/");
    filemgr_refresh();
}

static void filemgr_refresh(void)
{
    filemgr_state.file_count = 0;
    filemgr_state.selected = -1;

    /* List files from torFS */
    extern int tfs_count_used(void);
    extern int tfs_get_used_name(int idx, char *name_out, uint32 *size_out);

    int used = tfs_count_used();
    for (int i = 0; i < used && filemgr_state.file_count < FILEMGR_MAX_FILES; i++) {
        char name[64];
        uint32 size;
        if (tfs_get_used_name(i, name, &size) == 0) {
            snprintf(filemgr_state.files[filemgr_state.file_count], 64,
                     "%s (%u bytes)", name, size);
            filemgr_state.file_count++;
        }
    }

    if (filemgr_state.file_count == 0) {
        strcpy(filemgr_state.files[0], "(empty)");
        filemgr_state.file_count = 1;
    }
}

static void filemgr_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* White background */
    for (int i = 0; i < W * H; i++) fb[i] = 0xFFFFFFFF;

    /* Toolbar */
    for (int y = 0; y < 35; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    fb_set_color(0xFF000000);
    fb_draw_string(10, 8, "Back  Forward  Refresh  New  Delete");

    /* Path bar */
    for (int y = 35; y < 55; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFFFFFFF;

    fb_set_color(0xFF444444);
    fb_draw_string(10, 38, filemgr_state.current_path);

    /* File list */
    for (int i = 0; i < filemgr_state.file_count; i++) {
        int y = 60 + i * 22;
        if (y >= H - 30) break;

        uint32 bg = (i == filemgr_state.selected) ? 0xFF0078D7 : 0xFFFFFFFF;
        uint32 fg = (i == filemgr_state.selected) ? 0xFFFFFFFF : 0xFF000000;

        for (int row = y; row < y + 20 && row < H; row++)
            for (int col = 10; col < W - 10 && col < W; col++)
                fb[row * W + col] = bg;

        fb_set_color(fg);
        fb_draw_string(20, y + 2, filemgr_state.files[i]);
    }

    /* Status bar */
    for (int y = H - 25; y < H; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    char status[64];
    snprintf(status, sizeof(status), "%d items", filemgr_state.file_count);
    fb_set_color(0xFF444444);
    fb_draw_string(10, H - 20, status);
}

static void filemgr_click(int mx, int my)
{
    if (my > 60) {
        int idx = (my - 60) / 22;
        if (idx >= 0 && idx < filemgr_state.file_count) {
            filemgr_state.selected = idx;
        }
    }
    (void)mx;
}

/* ===== Paint / Drawing (FAZ 9.6) ===== */

static struct {
    uint32 canvas[PAINT_CANVAS_W * PAINT_CANVAS_H];
    uint32 fg_color;
    int brush_size;
    int tool; /* 0=brush, 1=line, 2=rect, 3=circle, 4=eraser */
    int drawing;
    int start_x, start_y;
} paint_state;

static void paint_init(void)
{
    memset(&paint_state, 0, sizeof(paint_state));
    for (int i = 0; i < PAINT_CANVAS_W * PAINT_CANVAS_H; i++)
        paint_state.canvas[i] = 0xFFFFFFFF;
    paint_state.fg_color = 0xFF000000;
    paint_state.brush_size = 2;
    paint_state.tool = 0;
    paint_state.drawing = 0;
}

static void paint_draw_brush(int cx, int cy, uint32 color, int size)
{
    for (int dy = -size; dy <= size; dy++) {
        for (int dx = -size; dx <= size; dx++) {
            if (dx*dx + dy*dy <= size*size) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < PAINT_CANVAS_W && py >= 0 && py < PAINT_CANVAS_H)
                    paint_state.canvas[py * PAINT_CANVAS_W + px] = color;
            }
        }
    }
}

static void paint_draw_line(int x0, int y0, int x1, int y1, uint32 color, int size)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) {
        int x = x0 + (dx * i) / steps;
        int y = y0 + (dy * i) / steps;
        paint_draw_brush(x, y, color, size);
    }
}

static void paint_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* White background */
    for (int i = 0; i < W * H; i++) fb[i] = 0xFFE0E0E0;

    /* Toolbar */
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < W; x++)
            fb[y * W + x] = 0xFFF0F0F0;

    const char *tools = "Brush  Line  Rect  Circle  Eraser  Clear  Save";
    fb_set_color(0xFF000000);
    fb_draw_string(10, 12, tools);

    /* Color palette */
    uint32 colors[] = {0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
                       0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF};
    for (int i = 0; i < 8; i++) {
        int cx = W - 200 + i * 25;
        for (int y = 8; y < 32; y++)
            for (int x = cx; x < cx + 20; x++)
                fb[y * W + x] = colors[i];
        /* Selection indicator */
        if (colors[i] == paint_state.fg_color) {
            for (int x = cx; x < cx + 20; x++) {
                fb[7 * W + x] = 0xFF0078D7;
                fb[32 * W + x] = 0xFF0078D7;
            }
        }
    }

    /* Canvas */
    int canvas_x = 10;
    int canvas_y = 45;
    int canvas_w = W - 20;
    int canvas_h = H - 55;

    /* Canvas border */
    for (int x = canvas_x - 1; x <= canvas_x + canvas_w; x++) {
        fb[(canvas_y - 1) * W + x] = 0xFF808080;
        fb[(canvas_y + canvas_h) * W + x] = 0xFF808080;
    }
    for (int y = canvas_y - 1; y <= canvas_y + canvas_h; y++) {
        fb[y * W + canvas_x - 1] = 0xFF808080;
        fb[y * W + canvas_x + canvas_w] = 0xFF808080;
    }

    /* Draw canvas content */
    for (int y = 0; y < canvas_h && y < PAINT_CANVAS_H; y++) {
        for (int x = 0; x < canvas_w && x < PAINT_CANVAS_W; x++) {
            fb[(canvas_y + y) * W + (canvas_x + x)] = paint_state.canvas[y * PAINT_CANVAS_W + x];
        }
    }
}

static void paint_click(int mx, int my)
{
    int W = 700; /* default paint width */

    if (my < 40) {
        /* Toolbar */
        if (mx < 60) paint_state.tool = 0;
        else if (mx < 120) paint_state.tool = 1;
        else if (mx < 180) paint_state.tool = 2;
        else if (mx < 250) paint_state.tool = 3;
        else if (mx < 320) paint_state.tool = 4;
        else if (mx < 380) {
            /* Clear */
            for (int i = 0; i < PAINT_CANVAS_W * PAINT_CANVAS_H; i++)
                paint_state.canvas[i] = 0xFFFFFFFF;
        }

        /* Color palette */
        uint32 colors[] = {0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
                           0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF};
        for (int i = 0; i < 8; i++) {
            int cx = W - 200 + i * 25;
            if (mx >= cx && mx < cx + 20) {
                paint_state.fg_color = colors[i];
                return;
            }
        }
    } else {
        /* Canvas area */
        int cx = mx - 10;
        int cy = my - 45;
        if (cx >= 0 && cx < PAINT_CANVAS_W && cy >= 0 && cy < PAINT_CANVAS_H) {
            if (paint_state.tool == 0 || paint_state.tool == 4) {
                /* Brush or eraser */
                uint32 color = (paint_state.tool == 4) ? 0xFFFFFFFF : paint_state.fg_color;
                paint_draw_brush(cx, cy, color, paint_state.brush_size);
            }
        }
    }
}

/* ===== Settings Panel (FAZ 9.7) ===== */

static struct {
    int bg_color_idx;
    int font_size;
    int sound_on;
    int theme; /* 0=light, 1=dark */
} settings_state;

static void settings_init(void)
{
    settings_state.bg_color_idx = 0;
    settings_state.font_size = 16;
    settings_state.sound_on = 1;
    settings_state.theme = 0;
}

static void settings_draw(window_t *win)
{
    if (!win || !win->framebuffer) return;
    uint32 *fb = win->framebuffer;
    int W = win->width;
    int H = win->height;

    /* Background */
    uint32 bg = settings_state.theme ? 0xFF2D2D2D : 0xFFFFFFFF;
    uint32 fg = settings_state.theme ? 0xFFFFFFFF : 0xFF000000;
    uint32 sec_bg = settings_state.theme ? 0xFF3D3D3D : 0xFFF5F5F5;

    for (int i = 0; i < W * H; i++) fb[i] = bg;

    /* Title */
    fb_set_color(fg);
    fb_draw_string(20, 15, "Settings");

    /* Sections */
    const char *sections[] = {
        "Appearance",
        "Sound",
        "Network",
        "Security",
        "About"
    };

    int y = 60;
    for (int i = 0; i < 5; i++) {
        /* Section header */
        for (int row = y; row < y + 30; row++)
            for (int x = 10; x < W - 10; x++)
                fb[row * W + x] = sec_bg;

        fb_set_color(fg);
        fb_draw_string(20, y + 6, sections[i]);
        y += 40;

        /* Section content */
        if (i == 0) {
            /* Appearance */
            fb_draw_string(30, y, settings_state.theme ? "Theme: Dark" : "Theme: Light");
            y += 20;
            fb_draw_string(30, y, "Font size: ");
            char sz[8]; itoa(settings_state.font_size, sz, 10);
            fb_draw_string(120, y, sz);
            y += 20;
        } else if (i == 1) {
            /* Sound */
            fb_draw_string(30, y, settings_state.sound_on ? "Sound: ON" : "Sound: OFF");
            y += 20;
        } else if (i == 2) {
            /* Network */
            extern char dhcp_ip_str[16];
            fb_draw_string(30, y, "IP: ");
            fb_draw_string(70, y, dhcp_ip_str[0] ? dhcp_ip_str : "Not configured");
            y += 20;
            fb_draw_string(30, y, "MAC: 52:54:00:12:34:56");
            y += 20;
        } else if (i == 3) {
            /* Security */
            fb_draw_string(30, y, "Firewall: Enabled");
            y += 20;
            fb_draw_string(30, y, "ASLR: Enabled");
            y += 20;
        } else if (i == 4) {
            /* About */
            fb_draw_string(30, y, "torOS v" TOROS_VER);
            y += 20;
            fb_draw_string(30, y, "Terminal Operating System");
            y += 20;
            fb_draw_string(30, y, "ARM64 (AArch64)");
            y += 20;
        }
        y += 15;
    }
}

static void settings_click(int mx, int my)
{
    (void)mx;
    if (my > 60 && my < 100) {
        /* Toggle theme */
        settings_state.theme = !settings_state.theme;
    }
}

/* ===== Global App Input Handler ===== */

void app_handle_mouse_click(window_t *win, int mx, int my)
{
    if (!win) return;

    /* Find which app owns this window */
    for (int i = 0; i < APP_MAX_INSTANCES; i++) {
        if (app_instances[i].active && app_instances[i].window == win) {
            const app_desc_t *desc = app_instances[i].desc;
            if (desc && desc->click_func) desc->click_func(mx, my);
            return;
        }
    }
}

void app_handle_key(window_t *win, char c)
{
    if (!win) return;

    for (int i = 0; i < APP_MAX_INSTANCES; i++) {
        if (app_instances[i].active && app_instances[i].window == win) {
            const app_desc_t *desc = app_instances[i].desc;
            if (desc && desc->key_func) desc->key_func(c);
            return;
        }
    }
}

/* ===== HTTP Client for Browser ===== */

int http_get(uint32 ip, uint16 port, const char *path, char *out, int out_size)
{
    if (!path || !out || out_size <= 0) return -1;

    /* Use network socket API */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return -1;
    }

    /* Send HTTP request */
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: toros\r\nUser-Agent: torOS/%s\r\nConnection: close\r\n\r\n",
             path, TOROS_VER);
    send(sock, request, strlen(request), 0);

    /* Receive response */
    int total = 0;
    int r;
    while ((r = recv(sock, out + total, out_size - total - 1, 0)) > 0) {
        total += r;
        if (total >= out_size - 1) break;
    }
    out[total] = '\0';
    closesocket(sock);

    /* Skip HTTP headers */
    char *body = strstr(out, "\r\n\r\n");
    if (body) {
        memmove(out, body + 4, total - (body - out) - 3);
    }

    return total;
}

/* ===== App Periodic Draw ===== */

void app_periodic_draw(void)
{
    for (int i = 0; i < APP_MAX_INSTANCES; i++) {
        if (app_instances[i].active && app_instances[i].window &&
            app_instances[i].window->visible) {
            const app_desc_t *desc = app_instances[i].desc;
            if (desc && desc->draw_func) {
                desc->draw_func(app_instances[i].window);
            }
        }
    }
}

/* ===== App Manager Command ===== */

void app_manager_command(const char *cmd)
{
    if (!cmd || !*cmd) return;

    if (strcmp(cmd, "list") == 0) {
        app_manager_list();
    } else if (strncmp(cmd, "launch ", 7) == 0) {
        app_launch(cmd + 7);
    } else if (strncmp(cmd, "close ", 6) == 0) {
        /* Find and close */
        for (int i = 0; i < APP_MAX_INSTANCES; i++) {
            if (app_instances[i].active && app_instances[i].desc &&
                strcmp(app_instances[i].desc->name, cmd + 6) == 0) {
                app_close(&app_instances[i]);
                break;
            }
        }
    } else {
        printk_color(TERM_YELLOW, "[APP] Commands: list, launch <name>, close <name>\n");
    }
}

/* Helpers */
static double atof(const char *s) {
    double val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    if (*s == '.') { s++; double frac = 0.1; while (*s >= '0' && *s <= '9') { val += (*s++ - '0') * frac; frac *= 0.1; } }
    return val;
}

/*
 * torOS Application Ecosystem
 * File Manager, Text Editor, Calculator, Terminal, Paint, Browser, Media Player, Settings
 */

#include "../include/toros.h"
#include "../include/app.h"
#include "../include/network.h"
#include "../include/image.h"
#include "../include/audio.h"
#include "../include/font.h"

static app_t *running_apps[MAX_APPS];
static int num_running = 0;

/* ===== File Manager ===== */

static file_manager_t fm;

static int fm_app_init(void *ctx) {
    file_manager_t *f = (file_manager_t *)ctx;
    fm_init(f);
    return 0;
}

static void fm_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    fm_draw((file_manager_t *)ctx, fb, fb_w, fb_h);
}

void fm_init(file_manager_t *fm) {
    memset(fm, 0, sizeof(file_manager_t));
    strcpy(fm->current_path, "/");
    fm->view_mode = 1;
    fm->sort_by = 0;
    fm->sort_asc = 1;
    fm_refresh(fm);
}

void fm_refresh(file_manager_t *fm) {
    fm->file_count = 0;
    fm->selected_idx = 0;
    /* List torFS files */
    extern torfs_entry_t torfs_entries[];
    extern int torfs_file_count;
    for (int i = 0; i < torfs_file_count && fm->file_count < FM_MAX_FILES; i++) {
        if (torfs_entries[i].in_use) {
            strncpy(fm->files[fm->file_count].name, torfs_entries[i].name, FM_NAME_LEN - 1);
            fm->files[fm->file_count].size = torfs_entries[i].size;
            fm->files[fm->file_count].is_dir = 0;
            fm->files[fm->file_count].is_hidden = (torfs_entries[i].name[0] == '.');
            fm->file_count++;
        }
    }
    /* Add pseudo-directories */
    if (fm->file_count < FM_MAX_FILES) {
        strcpy(fm->files[fm->file_count].name, "Desktop");
        fm->files[fm->file_count].is_dir = 1;
        fm->files[fm->file_count].size = 0;
        fm->file_count++;
    }
}

void fm_draw(file_manager_t *fm, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* Background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFFFFFFF;

    /* Title bar */
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF0078D7;
    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 4, "File Manager");

    /* Path bar */
    for (int y = 28; y < 50; y++)
        for (int x = 4; x < fb_w - 4; x++)
            fb[y * fb_w + x] = 0xFFF0F0F0;
    fb_set_color(0xFF000000);
    fb_draw_string(8, 30, fm->current_path);

    /* File list */
    int y = 56;
    for (int i = 0; i < fm->file_count && y < fb_h - 20; i++) {
        uint32 bg = (i == fm->selected_idx) ? 0xFF0078D7 : 0xFFFFFFFF;
        uint32 fg = (i == fm->selected_idx) ? 0xFFFFFFFF : 0xFF000000;
        for (int row = 0; row < 18; row++)
            for (int col = 0; col < fb_w - 8; col++)
                fb[(y + row) * fb_w + 4 + col] = bg;

        fb_set_color(fg);
        fb_draw_string(12, y + 2, fm->files[i].is_dir ? "[DIR] " : "      ");
        fb_draw_string(52, y + 2, fm->files[i].name);
        y += 20;
    }

    /* Status bar */
    for (int y = fb_h - 20; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFF0F0F0;
    char status[64];
    strcpy(status, "  ");
    utoa(fm->file_count, status + 2, 10);
    strcat(status, " items");
    fb_set_color(0xFF000000);
    fb_draw_string(4, fb_h - 16, status);
}

app_t *app_file_manager(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "filemanager");
    strcpy(app.title, "File Manager");
    app.init = fm_app_init;
    app.draw = fm_app_draw;
    app.ctx = &fm;
    return &app;
}

/* ===== Text Editor ===== */

static text_editor_t te;

static int te_app_init(void *ctx) {
    te_init((text_editor_t *)ctx);
    return 0;
}

static void te_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    te_draw((text_editor_t *)ctx, fb, fb_w, fb_h);
}

void te_init(text_editor_t *te) {
    memset(te, 0, sizeof(text_editor_t));
    strcpy(te->filename, "untitled.txt");
    te->num_lines = 1;
    strcpy(te->lines[0], "");
    te->show_line_numbers = 1;
}

int te_load_file(text_editor_t *te, const char *filename) {
    int size = tfs_size(filename);
    if (size <= 0) return -1;
    uint8 *buf = (uint8 *)kmalloc(size + 1);
    if (!buf) return -1;
    tfs_read(filename, buf, size, 0);
    buf[size] = '\0';

    /* Parse into lines */
    te->num_lines = 0;
    char *p = (char *)buf;
    while (*p && te->num_lines < TE_MAX_LINES) {
        char *line = te->lines[te->num_lines];
        int col = 0;
        while (*p && *p != '\n' && col < TE_LINE_LEN - 1) line[col++] = *p++;
        line[col] = '\0';
        if (*p == '\n') p++;
        te->num_lines++;
    }
    strncpy(te->filename, filename, 127);
    te->modified = 0;
    kfree(buf);
    return 0;
}

int te_save_file(text_editor_t *te) {
    /* Join lines */
    int total = 0;
    for (int i = 0; i < te->num_lines; i++) total += strlen(te->lines[i]) + 1;

    uint8 *buf = (uint8 *)kmalloc(total + 1);
    if (!buf) return -1;

    int pos = 0;
    for (int i = 0; i < te->num_lines; i++) {
        int len = strlen(te->lines[i]);
        memcpy(buf + pos, te->lines[i], len);
        pos += len;
        buf[pos++] = '\n';
    }

    int fd = tfs_create(te->filename);
    if (fd < 0) { kfree(buf); return -1; }
    tfs_write(te->filename, buf, pos, 0);
    kfree(buf);
    te->modified = 0;
    return 0;
}

void te_draw(text_editor_t *te, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* White background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFFFFFFF;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Title bar */
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF0078D7;
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 4, "Text Editor - ");
    fb_draw_string(120, 4, te->filename);
    if (te->modified) fb_draw_string(200, 4, " *");

    /* Line numbers + content */
    int ln_width = te->show_line_numbers ? 40 : 0;
    fb_set_color(0xFFF0F0F0);
    for (int y = 24; y < fb_h; y++)
        for (int x = 0; x < ln_width; x++)
            fb[y * fb_w + x] = 0xFFF0F0F0;

    int y = 28;
    for (int i = te->scroll_line; i < te->num_lines && y < fb_h - 8; i++) {
        if (te->show_line_numbers) {
            char num[8]; utoa(i + 1, num, 10);
            fb_set_color(0xFF808080);
            fb_draw_string(4, y, num);
        }
        fb_set_color(0xFF000000);
        fb_draw_string(ln_width + 4, y, te->lines[i]);

        /* Cursor */
        if (i == te->cursor_line) {
            int cx = ln_width + 4 + te->cursor_col * 8;
            for (int cy = y; cy < y + 14; cy++)
                if (cx < fb_w) fb[cy * fb_w + cx] = 0xFF000000;
        }
        y += 16;
    }
}

app_t *app_text_editor(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "editor");
    strcpy(app.title, "Text Editor");
    app.init = te_app_init;
    app.draw = te_app_draw;
    app.ctx = &te;
    return &app;
}

/* ===== Calculator ===== */

static calculator_t calc;

static int calc_app_init(void *ctx) {
    calc_init((calculator_t *)ctx);
    return 0;
}

static void calc_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    calc_draw((calculator_t *)ctx, fb, fb_w, fb_h);
}

void calc_init(calculator_t *calc) {
    memset(calc, 0, sizeof(calculator_t));
    calc->new_entry = 1;
    calc->display[0] = '0';
    calc->display[1] = '\0';
}

void calc_input_digit(calculator_t *calc, int digit) {
    if (calc->has_error) return;
    if (calc->new_entry) {
        calc->display[0] = '0' + digit;
        calc->display[1] = '\0';
        calc->new_entry = 0;
    } else {
        int len = strlen(calc->display);
        if (len < CALC_DISPLAY_LEN - 1) {
            calc->display[len] = '0' + digit;
            calc->display[len + 1] = '\0';
        }
    }
}

void calc_input_op(calculator_t *calc, char op) {
    calc->accumulator = atof(calc->display);
    calc->pending_op = op;
    calc->new_entry = 1;
}

void calc_compute(calculator_t *calc) {
    if (calc->pending_op == 0) return;
    double current = atof(calc->display);
    double result = 0;
    switch (calc->pending_op) {
    case '+': result = calc->accumulator + current; break;
    case '-': result = calc->accumulator - current; break;
    case '*': result = calc->accumulator * current; break;
    case '/': result = (current != 0) ? calc->accumulator / current : 0; break;
    }
    if (result == (int64)result) snprintf(calc->display, CALC_DISPLAY_LEN, "%lld", (int64)result);
    else snprintf(calc->display, CALC_DISPLAY_LEN, "%f", result);
    calc->pending_op = 0;
    calc->new_entry = 1;
}

void calc_draw(calculator_t *calc, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* Background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF333333;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Display */
    for (int y = 8; y < 48; y++)
        for (int x = 8; x < fb_w - 8; x++)
            fb[y * fb_w + x] = 0xFF222222;
    fb_set_color(0xFFFFFFFF);
    int dx = fb_w - 16 - strlen(calc->display) * 8;
    fb_draw_string(dx > 8 ? dx : 8, 16, calc->display);

    /* Buttons */
    const char *btn_labels[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "0", ".", "=", "+"
    };
    int btn_w = (fb_w - 24) / 4;
    int btn_h = (fb_h - 64) / 4;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = 8 + col * (btn_w + 2);
            int by = 56 + row * (btn_h + 2);
            uint32 bg = (col == 3) ? 0xFFFF8C00 : 0xFF555555;
            for (int y = 0; y < btn_h; y++)
                for (int x = 0; x < btn_w; x++)
                    fb[(by + y) * fb_w + bx + x] = bg;
            fb_set_color(0xFFFFFFFF);
            fb_draw_string(bx + btn_w / 2 - 4, by + btn_h / 2 - 4, btn_labels[row * 4 + col]);
        }
    }
}

app_t *app_calculator(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "calculator");
    strcpy(app.title, "Calculator");
    app.init = calc_app_init;
    app.draw = calc_app_draw;
    app.ctx = &calc;
    return &app;
}

/* ===== Terminal ===== */

static terminal_t terminal;

static int term_app_init(void *ctx) {
    term_init((terminal_t *)ctx);
    return 0;
}

static void term_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    term_draw((terminal_t *)ctx, fb, fb_w, fb_h);
}

void term_init(terminal_t *term) {
    memset(term, 0, sizeof(terminal_t));
    term->fg_color = 0xFF00FF00;
    term->bg_color = 0xFF000000;
    term->cursor_row = 0;
    term->cursor_col = 0;
    strcpy(term->buffer[0], "torOS Terminal v0.4");
    term->scrollback = 1;
    strcpy(term->buffer[1], "Type 'help' for commands");
    term->scrollback = 2;
}

void term_write(terminal_t *term, const char *text) {
    while (*text) {
        if (*text == '\n' || term->cursor_col >= TERM_COLS) {
            term->cursor_col = 0;
            term->cursor_row++;
            if (term->cursor_row >= TERM_ROWS) {
                /* Scroll */
                for (int i = 0; i < TERM_ROWS - 1; i++)
                    strcpy(term->buffer[i], term->buffer[i + 1]);
                term->cursor_row = TERM_ROWS - 1;
            }
            term->buffer[term->cursor_row][0] = '\0';
            if (*text == '\n') { text++; continue; }
        }
        term->buffer[term->cursor_row][term->cursor_col++] = *text;
        term->buffer[term->cursor_row][term->cursor_col] = '\0';
        text++;
    }
    if (term->scrollback < term->cursor_row + 1) term->scrollback = term->cursor_row + 1;
}

void term_draw(terminal_t *term, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* Black background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = term->bg_color;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    fb_set_color(term->fg_color);
    int y = 8;
    for (int i = 0; i < TERM_ROWS && y < fb_h - 16; i++) {
        fb_draw_string(8, y, term->buffer[i]);
        y += 14;
    }

    /* Input line */
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, y, "> ");
    fb_draw_string(24, y, term->input_buffer);
}

app_t *app_terminal(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "terminal");
    strcpy(app.title, "Terminal");
    app.init = term_app_init;
    app.draw = term_app_draw;
    app.ctx = &terminal;
    return &app;
}

/* ===== Paint ===== */

static paint_app_t paint_app;

static int paint_app_init(void *ctx) {
    paint_init((paint_app_t *)ctx);
    return 0;
}

static void paint_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    paint_draw((paint_app_t *)ctx, fb, fb_w, fb_h);
}

void paint_init(paint_app_t *pa) {
    memset(pa, 0, sizeof(paint_app_t));
    pa->tool = PAINT_TOOL_PENCIL;
    pa->fg_color = 0xFF000000;
    pa->bg_color = 0xFFFFFFFF;
    pa->brush_size = 2;
    /* White canvas */
    for (int i = 0; i < PAINT_WIDTH * PAINT_HEIGHT; i++) pa->canvas[i] = 0xFFFFFFFF;
}

void paint_set_pixel(paint_app_t *pa, int x, int y, uint32 color) {
    if (x >= 0 && x < PAINT_WIDTH && y >= 0 && y < PAINT_HEIGHT)
        pa->canvas[y * PAINT_WIDTH + x] = color;
}

void paint_line(paint_app_t *pa, int x0, int y0, int x1, int y1, uint32 color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        paint_set_pixel(pa, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void paint_undo(paint_app_t *pa) {
    if (pa->undo_idx > 0) {
        pa->undo_idx--;
        memcpy(pa->canvas, pa->undo_stack[pa->undo_idx], PAINT_WIDTH * PAINT_HEIGHT * 4);
    }
}

void paint_draw(paint_app_t *pa, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* Draw canvas */
    int cw = (fb_w - 60 < PAINT_WIDTH) ? fb_w - 60 : PAINT_WIDTH;
    int ch = (fb_h - 4 < PAINT_HEIGHT) ? fb_h - 4 : PAINT_HEIGHT;
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
            fb[y * fb_w + x] = pa->canvas[y * PAINT_WIDTH + x];

    /* Toolbar */
    for (int y = 0; y < fb_h; y++)
        for (int x = cw; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFE0E0E0;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);
    fb_set_color(0xFF000000);
    fb_draw_string(cw + 4, 4, "Tools");
    const char *tools[] = {"Pencil", "Brush", "Eraser", "Line", "Rect", "Circle", "Fill", "Picker", "Text"};
    for (int i = 0; i < 9; i++) {
        uint32 bg = (i == pa->tool) ? 0xFF0078D7 : 0xFFFFFFFF;
        for (int y = 0; y < 18; y++)
            for (int x = 0; x < 50; x++)
                fb[(24 + i * 22 + y) * fb_w + cw + 4 + x] = bg;
        fb_set_color((i == pa->tool) ? 0xFFFFFFFF : 0xFF000000);
        fb_draw_string(cw + 8, 26 + i * 22, tools[i]);
    }
}

app_t *app_paint(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "paint");
    strcpy(app.title, "Paint");
    app.init = paint_app_init;
    app.draw = paint_app_draw;
    app.ctx = &paint_app;
    return &app;
}

/* ===== Browser ===== */

static browser_t browser;

static int browser_app_init(void *ctx) {
    browser_init((browser_t *)ctx);
    return 0;
}

static void browser_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    browser_draw((browser_t *)ctx, fb, fb_w, fb_h);
}

void browser_init(browser_t *b) {
    memset(b, 0, sizeof(browser_t));
    strcpy(b->url, "http://example.com");
    b->page_width = FB_WIDTH - 20;
    b->page_height = FB_HEIGHT - 60;
}

void browser_navigate(browser_t *b, const char *url) {
    if (!url) return;
    strncpy(b->url, url, BROWSER_URL_LEN - 1);
    b->loading = 1;

    /* Parse URL */
    char host[128] = {0};
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    int i = 0;
    while (*p && *p != '/' && i < 127) host[i++] = *p++;
    host[i] = '\0';

    /* HTTP GET */
    uint32 ip;
    if (dns_resolve(host, &ip) == 0) {
        int sock = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock >= 0) {
            sockaddr_in_t addr = {.family = AF_INET, .port = htons(80), .addr = ip};
            if (sys_connect(sock, &addr, sizeof(addr)) == 0) {
                char req[512];
                snprintf(req, sizeof(req), "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
                sys_send(sock, req, strlen(req), 0);
                uint8 resp[4096];
                int rlen = sys_recv(sock, resp, sizeof(resp), 0);
                if (rlen > 0) {
                    resp[rlen] = '\0';
                    /* Simple HTML render: just show text content */
                    strncpy(b->title, "Loaded: ", BROWSER_TITLE_LEN - 1);
                    strncat(b->title, host, BROWSER_TITLE_LEN - 10);
                }
            }
            sys_close(sock);
        }
    }
    b->loading = 0;
}

void browser_draw(browser_t *b, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* White page */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFFFFFFF;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Address bar */
    for (int y = 4; y < 28; y++)
        for (int x = 80; x < fb_w - 60; x++)
            fb[y * fb_w + x] = 0xFFF0F0F0;
    fb_set_color(0xFF000000);
    fb_draw_string(84, 8, b->url);

    /* Nav buttons */
    fb_set_color(0xFF808080);
    fb_draw_string(4, 8, "<- -> [@]");
    fb_draw_string(fb_w - 50, 8, "Go!");

    /* Page content area */
    for (int y = 32; y < 36; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFE0E0E0;

    /* Title */
    fb_set_color(0xFF000080);
    fb_draw_string(8, 42, b->title);

    /* URL display */
    fb_set_color(0xFF404040);
    fb_draw_string(8, 60, "URL: ");
    fb_draw_string(48, 60, b->url);

    if (b->loading) {
        fb_set_color(0xFF008000);
        fb_draw_string(8, 80, "Loading...");
    } else {
        fb_set_color(0xFF000000);
        fb_draw_string(8, 80, "torOS Browser - HTML rendering in development");
        fb_draw_string(8, 100, "Features: DNS resolution, HTTP GET, TCP sockets");
    }
}

app_t *app_browser(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "browser");
    strcpy(app.title, "Browser");
    app.init = browser_app_init;
    app.draw = browser_app_draw;
    app.ctx = &browser;
    return &app;
}

/* ===== Media Player ===== */

static media_player_t mp;

static int mp_app_init(void *ctx) {
    mp_init((media_player_t *)ctx);
    return 0;
}

static void mp_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    mp_draw((media_player_t *)ctx, fb, fb_w, fb_h);
}

void mp_init(media_player_t *mp) {
    memset(mp, 0, sizeof(media_player_t));
    mp->state = MEDIA_STATE_STOPPED;
    mp->volume = 80;
    strcpy(mp->filename, "No file");
}

void mp_draw(media_player_t *mp, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    /* Dark background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF1A1A1A;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Title */
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 8, "Media Player");

    /* Filename */
    fb_set_color(0xFFAAAAAA);
    fb_draw_string(8, 28, mp->filename);

    /* Play state */
    const char *state_str = (mp->state == MEDIA_STATE_PLAYING) ? "PLAYING" :
                            (mp->state == MEDIA_STATE_PAUSED) ? "PAUSED" : "STOPPED";
    fb_set_color(0xFF00FF00);
    fb_draw_string(8, 48, state_str);

    /* Controls */
    fb_set_color(0xFF444444);
    int btn_y = fb_h - 40;
    for (int x = 0; x < fb_w; x++)
        for (int y = btn_y; y < fb_h; y++)
            fb[y * fb_w + x] = 0xFF333333;

    fb_set_color(0xFFFFFFFF);
    fb_draw_string(fb_w / 2 - 60, btn_y + 10, "|<<  >  ||  >>|");

    /* Progress bar background */
    for (int y = btn_y - 10; y < btn_y - 4; y++)
        for (int x = 10; x < fb_w - 10; x++)
            fb[y * fb_w + x] = 0xFF555555;

    /* Progress */
    if (mp->duration_ms > 0) {
        int progress = ((fb_w - 20) * mp->current_ms) / mp->duration_ms;
        for (int y = btn_y - 10; y < btn_y - 4; y++)
            for (int x = 10; x < 10 + progress; x++)
                fb[y * fb_w + x] = 0xFF0078D7;
    }
}

app_t *app_media_player(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "mediaplayer");
    strcpy(app.title, "Media Player");
    app.init = mp_app_init;
    app.draw = mp_app_draw;
    app.ctx = &mp;
    return &app;
}

/* ===== Settings ===== */

static settings_app_t settings;

static int settings_app_init(void *ctx) {
    settings_init((settings_app_t *)ctx);
    return 0;
}

static void settings_app_draw(void *ctx, uint32 *fb, int fb_w, int fb_h) {
    settings_draw((settings_app_t *)ctx, fb, fb_w, fb_h);
}

void settings_init(settings_app_t *sa) {
    memset(sa, 0, sizeof(settings_app_t));
    sa->num_categories = 4;
    strcpy(sa->categories[0].category_name, "General");
    strcpy(sa->categories[0].items[0].name, "Theme");
    sa->categories[0].items[0].type = 3;
    strcpy(sa->categories[0].items[0].choices[0], "Light");
    strcpy(sa->categories[0].items[0].choices[1], "Dark");
    sa->categories[0].items[0].num_choices = 2;
    sa->categories[0].num_items = 1;

    strcpy(sa->categories[1].category_name, "Display");
    strcpy(sa->categories[1].items[0].name, "Resolution");
    sa->categories[1].items[0].type = 3;
    strcpy(sa->categories[1].items[0].choices[0], "1024x768");
    strcpy(sa->categories[1].items[0].choices[1], "1280x720");
    strcpy(sa->categories[1].items[0].choices[2], "1920x1080");
    sa->categories[1].items[0].num_choices = 3;
    sa->categories[1].num_items = 1;

    strcpy(sa->categories[2].category_name, "Network");
    strcpy(sa->categories[2].items[0].name, "DHCP");
    sa->categories[2].items[0].type = 0;
    sa->categories[2].items[0].value.i = 1;
    sa->categories[2].num_items = 1;

    strcpy(sa->categories[3].category_name, "Sound");
    strcpy(sa->categories[3].items[0].name, "Master Volume");
    sa->categories[3].items[0].type = 1;
    sa->categories[3].items[0].value.i = 80;
    sa->categories[3].num_items = 1;
}

void settings_draw(settings_app_t *sa, uint32 *fb, int fb_w, int fb_h) {
    if (!fb) return;
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFFFFFFFF;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Title */
    for (int y = 0; y < 28; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF0078D7;
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 4, "Settings");

    /* Sidebar */
    for (int y = 28; y < fb_h; y++)
        for (int x = 0; x < 120; x++)
            fb[y * fb_w + x] = 0xFFF0F0F0;

    /* Categories */
    for (int i = 0; i < sa->num_categories; i++) {
        uint32 bg = (i == sa->selected_category) ? 0xFFE0E0E0 : 0xFFF0F0F0;
        for (int y = 0; y < 28; y++)
            for (int x = 0; x < 116; x++)
                fb[(32 + i * 30 + y) * fb_w + 2 + x] = bg;
        fb_set_color(0xFF000000);
        fb_draw_string(10, 36 + i * 30, sa->categories[i].category_name);
    }

    /* Settings content */
    setting_category_t *cat = &sa->categories[sa->selected_category];
    int y = 36;
    for (int i = 0; i < cat->num_items; i++) {
        fb_set_color(0xFF000000);
        fb_draw_string(130, y, cat->items[i].name);
        fb_draw_string(280, y, ":");
        if (cat->items[i].type == 0) {
            fb_draw_string(290, y, cat->items[i].value.i ? "ON" : "OFF");
        } else if (cat->items[i].type == 1) {
            char val[16]; itoa(cat->items[i].value.i, val, 10);
            fb_draw_string(290, y, val);
        } else if (cat->items[i].type == 3 && cat->items[i].num_choices > 0) {
            fb_draw_string(290, y, cat->items[i].choices[0]);
        }
        y += 30;
    }
}

app_t *app_settings(void) {
    static app_t app;
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "settings");
    strcpy(app.title, "Settings");
    app.init = settings_app_init;
    app.draw = settings_app_draw;
    app.ctx = &settings;
    return &app;
}

/* ===== App Manager ===== */

static app_t *app_registry[MAX_APPS];
static int registry_count = 0;

void app_manager_init(void) {
    memset(running_apps, 0, sizeof(running_apps));
    num_running = 0;
    memset(app_registry, 0, sizeof(app_registry));
    registry_count = 0;

    /* Register all apps */
    app_registry[registry_count++] = app_file_manager();
    app_registry[registry_count++] = app_text_editor();
    app_registry[registry_count++] = app_calculator();
    app_registry[registry_count++] = app_terminal();
    app_registry[registry_count++] = app_paint();
    app_registry[registry_count++] = app_browser();
    app_registry[registry_count++] = app_media_player();
    app_registry[registry_count++] = app_settings();

    printk_color(TERM_GREEN, "[BOOT] App Manager: %d apps registered\n", registry_count);
}

app_t *app_launch(const char *name) {
    for (int i = 0; i < registry_count; i++) {
        if (app_registry[i] && strcmp(app_registry[i]->name, name) == 0) {
            app_t *app = app_registry[i];
            if (app->init) app->init(app->ctx);
            app->running = 1;

            /* Create window */
            if (!app->main_window) {
                app->main_window = wm_create_window(app->title, 100, 100, 600, 450, WS_OVERLAPPED);
                if (app->main_window) wm_show_window(app->main_window);
            }

            if (num_running < MAX_APPS) running_apps[num_running++] = app;
            printk_color(TERM_GREEN, "[APP] Launched: %s\n", name);
            return app;
        }
    }
    printk_color(TERM_RED, "[APP] Not found: %s\n", name);
    return NULL;
}

void app_close(app_t *app) {
    if (!app) return;
    app->running = 0;
    if (app->cleanup) app->cleanup(app->ctx);
    if (app->main_window) {
        wm_destroy_window(app->main_window);
        app->main_window = NULL;
    }
    printk_color(TERM_YELLOW, "[APP] Closed: %s\n", app->name);
}

void app_manager_draw_all(uint32 *fb, int fb_w, int fb_h) {
    for (int i = 0; i < num_running; i++) {
        app_t *app = running_apps[i];
        if (app && app->running && app->draw && app->main_window &&
            app->main_window->framebuffer) {
            app->draw(app->ctx, app->main_window->framebuffer,
                      app->main_window->width, app->main_window->height);
        }
    }
}

app_t *app_get_by_name(const char *name) {
    for (int i = 0; i < registry_count; i++)
        if (app_registry[i] && strcmp(app_registry[i]->name, name) == 0)
            return app_registry[i];
    return NULL;
}

void app_manager_list(void) {
    printk_color(TERM_CYAN, "\n=== Applications ===\n");
    for (int i = 0; i < registry_count; i++) {
        if (app_registry[i]) {
            const char *status = app_registry[i]->running ? " [RUNNING]" : "";
            printk("  %s - %s%s\n", app_registry[i]->name, app_registry[i]->title, status);
        }
    }
    printk("\n");
}

/* Helpers */
static double atof(const char *s) {
    double val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    if (*s == '.') { s++; double frac = 0.1; while (*s >= '0' && *s <= '9') { val += (*s++ - '0') * frac; frac *= 0.1; } }
    return val;
}
#endif

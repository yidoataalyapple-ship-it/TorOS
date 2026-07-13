/*
 * torOS Desktop Shell
 * Wallpaper, taskbar, start menu, system clock
 */

#include "../include/toros.h"
#include "../include/window.h"
#include "../include/gpu.h"

static desktop_shell_t shell;

/* Default gradient wallpaper (blue-torOS theme) */
static void generate_default_wallpaper(uint32 width, uint32 height)
{
    if (!shell.wallpaper)
        return;

    for (uint32 y = 0; y < height; y++) {
        for (uint32 x = 0; x < width; x++) {
            /* Gradient from dark blue to lighter blue */
            uint8 r = (x * 20) / width + 10;
            uint8 g = (y * 30) / height + 40;
            uint8 b = 80 + (y * 60) / height;

            /* Subtle pattern */
            if ((x / 16 + y / 16) % 2 == 0) {
                r = (r * 95) / 100;
                g = (g * 95) / 100;
                b = (b * 95) / 100;
            }

            shell.wallpaper[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    /* torOS logo text area */
    const char *logo = "torOS";
    uint32 logo_x = width / 2 - 30;
    uint32 logo_y = height / 2 - 20;

    for (uint32 row = 0; row < 30; row++) {
        for (uint32 col = 0; col < 120; col++) {
            uint32 px = logo_x + col;
            uint32 py = logo_y + row;
            if (px < width && py < height) {
                /* Semi-transparent dark box */
                uint8 alpha = 180;
                uint32 bg = shell.wallpaper[py * width + px];
                uint8 br = ((bg >> 16) & 0xFF) * (255 - alpha) / 255;
                uint8 bg_g = ((bg >> 8) & 0xFF) * (255 - alpha) / 255;
                uint8 bb = (bg & 0xFF) * (255 - alpha) / 255;
                shell.wallpaper[py * width + px] = 0xFF000000 | (br << 16) | (bg_g << 8) | bb;
            }
        }
    }
}

void desktop_shell_init(uint32 width, uint32 height)
{
    printk_color(TERM_YELLOW, "[BOOT] Desktop Shell...\n");

    memset(&shell, 0, sizeof(desktop_shell_t));
    spin_init(&shell.shell_lock);

    shell.taskbar_height = 40;
    shell.taskbar_color = 0xFF1A1A2E;
    shell.taskbar_accent = 0xFF0078D7;
    shell.wallpaper_width = width;
    shell.wallpaper_height = height - shell.taskbar_height;

    /* Allocate wallpaper */
    shell.wallpaper = (uint32 *)kmalloc(width * height * sizeof(uint32));
    if (shell.wallpaper) {
        generate_default_wallpaper(width, height);
    }

    /* Start menu */
    shell.start_menu_width = 300;
    shell.start_menu_height = 400;
    shell.start_menu_x = 0;
    shell.start_menu_y = height - shell.taskbar_height - shell.start_menu_height;

    /* Clock position */
    shell.clock_x = width - 80;
    shell.clock_y = height - shell.taskbar_height + 10;

    shell.initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] Desktop Shell: taskbar=%dpx, wallpaper=%dx%d\n",
                 shell.taskbar_height, width, height);
}

void desktop_shell_draw(uint32 *fb, int fb_w, int fb_h)
{
    if (!shell.initialized || !fb)
        return;

    /* Draw wallpaper */
    if (shell.wallpaper) {
        for (int y = 0; y < fb_h - (int)shell.taskbar_height && y < (int)shell.wallpaper_height; y++) {
            for (int x = 0; x < fb_w; x++) {
                fb[y * fb_w + x] = shell.wallpaper[y * shell.wallpaper_width + x];
            }
        }
    } else {
        /* Fallback solid color */
        for (int y = 0; y < fb_h - (int)shell.taskbar_height; y++) {
            for (int x = 0; x < fb_w; x++) {
                fb[y * fb_w + x] = 0xFF0078D7;
            }
        }
    }
}

void desktop_shell_draw_taskbar(uint32 *fb, int fb_w, int fb_h)
{
    if (!shell.initialized || !fb)
        return;

    int tb_y = fb_h - shell.taskbar_height;

    /* Taskbar background */
    for (int y = tb_y; y < fb_h; y++) {
        for (int x = 0; x < fb_w; x++) {
            fb[y * fb_w + x] = shell.taskbar_color;
        }
    }

    /* Taskbar top border line */
    for (int x = 0; x < fb_w; x++) {
        fb[tb_y * fb_w + x] = 0xFF333355;
    }

    /* Start button */
    int start_btn_w = 60;
    int start_btn_h = shell.taskbar_height - 8;
    int start_btn_x = 4;
    int start_btn_y = tb_y + 4;

    /* Start button background */
    for (int y = 0; y < start_btn_h; y++) {
        for (int x = 0; x < start_btn_w; x++) {
            int px = start_btn_x + x;
            int py = start_btn_y + y;

            if (shell.start_menu_open) {
                fb[py * fb_w + px] = 0xFF333355;  /* Pressed */
            } else {
                fb[py * fb_w + px] = 0xFF0078D7;  /* Normal */
            }
        }
    }

    /* Start button text */
    extern void fb_draw_string(int x, int y, const char *s);
    extern void fb_set_color(uint32 color);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(start_btn_x + 8, start_btn_y + 6, "Start");

    /* Taskbar buttons for open windows */
    extern window_manager_t wm;
    int btn_x = start_btn_x + start_btn_w + 10;
    window_t *win = wm.window_list;

    while (win) {
        if ((win->flags & WS_VISIBLE) && win->state != WSTATE_MINIMIZED &&
            win->state != WSTATE_HIDDEN && win != wm.desktop_window) {

            int wbtn_w = 120;
            int wbtn_h = shell.taskbar_height - 10;

            if (btn_x + wbtn_w > shell.clock_x - 10)
                break;

            /* Button background */
            uint32 btn_color = (win == wm.active_window) ? 0xFF333355 : shell.taskbar_color;
            for (int y = 0; y < wbtn_h; y++) {
                for (int x = 0; x < wbtn_w; x++) {
                    int px = btn_x + x;
                    int py = tb_y + 5 + y;
                    fb[py * fb_w + px] = btn_color;
                }
            }

            /* Window title */
            fb_set_color(0xFFFFFFFF);
            char title[20];
            strncpy(title, win->title, 15);
            title[15] = '\0';
            fb_draw_string(btn_x + 4, tb_y + 10, title);

            btn_x += wbtn_w + 2;
        }
        win = win->next;
    }

    /* System tray / clock area */
    for (int x = shell.clock_x - 10; x < fb_w; x++) {
        for (int y = tb_y + 2; y < fb_h - 2; y++) {
            fb[y * fb_w + x] = 0xFF151530;
        }
    }

    /* Clock */
    desktop_shell_draw_clock(fb, fb_w, fb_h);

    /* Start menu (if open) */
    if (shell.start_menu_open) {
        desktop_shell_draw_start_menu(fb, fb_w, fb_h);
    }
}

void desktop_shell_draw_start_menu(uint32 *fb, int fb_w, int fb_h)
{
    if (!shell.start_menu_open || !fb)
        return;

    int sm_x = shell.start_menu_x;
    int sm_y = shell.start_menu_y;
    int sm_w = shell.start_menu_width;
    int sm_h = shell.start_menu_height;

    /* Clip to screen */
    if (sm_y < 0) sm_y = 0;

    /* Menu background */
    for (int y = 0; y < sm_h && (sm_y + y) < fb_h; y++) {
        for (int x = 0; x < sm_w && (sm_x + x) < fb_w; x++) {
            int px = sm_x + x;
            int py = sm_y + y;

            /* Semi-transparent dark background */
            uint32 bg = fb[py * fb_w + px];
            uint8 r = ((bg >> 16) & 0xFF) * 30 / 100;
            uint8 g = ((bg >> 8) & 0xFF) * 30 / 100;
            uint8 b = (bg & 0xFF) * 30 / 100;
            fb[py * fb_w + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    /* Menu border */
    for (int x = 0; x < sm_w && (sm_x + x) < fb_w; x++) {
        int top_y = sm_y;
        int bot_y = sm_y + sm_h - 1;
        if (top_y >= 0 && top_y < fb_h) fb[top_y * fb_w + sm_x + x] = 0xFF444466;
        if (bot_y >= 0 && bot_y < fb_h) fb[bot_y * fb_w + sm_x + x] = 0xFF444466;
    }
    for (int y = 0; y < sm_h && (sm_y + y) < fb_h; y++) {
        int left_x = sm_x;
        int right_x = sm_x + sm_w - 1;
        if (left_x >= 0 && left_x < fb_w && (sm_y + y) >= 0 && (sm_y + y) < fb_h) {
            fb[(sm_y + y) * fb_w + left_x] = 0xFF444466;
            if (right_x < fb_w) fb[(sm_y + y) * fb_w + right_x] = 0xFF444466;
        }
    }

    /* Menu header */
    for (int x = 1; x < sm_w - 1 && (sm_x + x) < fb_w; x++) {
        for (int y = 1; y < 40 && (sm_y + y) < fb_h; y++) {
            fb[(sm_y + y) * fb_w + sm_x + x] = 0xFF0078D7;
        }
    }

    extern void fb_draw_string(int x, int y, const char *s);
    extern void fb_set_color(uint32 color);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(sm_x + 10, sm_y + 12, "torOS Start Menu");

    /* Menu items */
    const char *items[] = {
        "File Manager",
        "Text Editor",
        "Terminal",
        "Calculator",
        "Settings",
        "-",
        "Log Out",
        "Shut Down",
        NULL
    };

    int item_y = sm_y + 50;
    for (int i = 0; items[i]; i++) {
        if (strcmp(items[i], "-") == 0) {
            /* Separator */
            for (int x = 10; x < sm_w - 10; x++) {
                fb[item_y * fb_w + sm_x + x] = 0xFF555577;
            }
            item_y += 10;
        } else {
            fb_set_color(0xFFCCCCDD);
            fb_draw_string(sm_x + 15, item_y, items[i]);
            item_y += 30;
        }
    }
}

void desktop_shell_draw_clock(uint32 *fb, int fb_w, int fb_h)
{
    if (!fb)
        return;

    /* Update clock string if needed */
    uint64 now = get_jiffies();
    if (now - shell.last_clock_update >= 10) {  /* Every 100ms */
        desktop_shell_update_clock();
        shell.last_clock_update = now;
    }

    extern void fb_set_color(uint32 color);
    extern void fb_draw_string(int x, int y, const char *s);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(shell.clock_x, shell.clock_y, shell.clock_str);
}

void desktop_shell_update_clock(void)
{
    uint64 seconds = rtc_get_time();
    uint32 hours = (seconds / 3600) % 24;
    uint32 minutes = (seconds / 60) % 60;

    shell.clock_str[0] = '0' + (hours / 10);
    shell.clock_str[1] = '0' + (hours % 10);
    shell.clock_str[2] = ':';
    shell.clock_str[3] = '0' + (minutes / 10);
    shell.clock_str[4] = '0' + (minutes % 10);
    shell.clock_str[5] = '\0';
}

void desktop_shell_toggle_start_menu(void)
{
    shell.start_menu_open = !shell.start_menu_open;
}

void desktop_shell_set_wallpaper(const uint32 *bitmap, uint32 w, uint32 h)
{
    if (!bitmap || !shell.wallpaper)
        return;

    /* Scale/crop to desktop size */
    for (uint32 y = 0; y < shell.wallpaper_height && y < h; y++) {
        for (uint32 x = 0; x < shell.wallpaper_width && x < w; x++) {
            shell.wallpaper[y * shell.wallpaper_width + x] = bitmap[y * w + x];
        }
    }
}

void desktop_shell_click(int x, int y)
{
    int hit = desktop_shell_hit_test(x, y);

    switch (hit) {
    case DSHT_START:
        desktop_shell_toggle_start_menu();
        break;
    case DSHT_CLOCK:
        printk_color(TERM_CYAN, "[SHELL] Clock clicked\n");
        break;
    default:
        /* Close start menu if clicked elsewhere */
        if (shell.start_menu_open)
            shell.start_menu_open = 0;
        break;
    }
}

int desktop_shell_hit_test(int x, int y)
{
    int tb_y = shell.wallpaper_height;

    /* Check if in taskbar */
    if (y < tb_y || y >= (int)(tb_y + shell.taskbar_height))
        return DSHT_NONE;

    /* Start button */
    if (x >= 4 && x < 64)
        return DSHT_START;

    /* Clock */
    if (x >= (int)shell.clock_x - 10)
        return DSHT_CLOCK;

    /* Taskbar window buttons */
    int btn_x = 74;
    extern window_manager_t wm;
    window_t *win = wm.window_list;

    while (win) {
        if ((win->flags & WS_VISIBLE) && win->state != WSTATE_MINIMIZED &&
            win->state != WSTATE_HIDDEN && win != wm.desktop_window) {
            if (x >= btn_x && x < btn_x + 120)
                return DSHT_WINDOW_BTN;
            btn_x += 122;
        }
        win = win->next;
    }

    return DSHT_TASKBAR;
}

/******************************************************************************
 * torOS - Terminal Operating System
 * Shell GUI - Graphical Shell Mode
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/window.h"
#include "../include/widget.h"
#include "../include/font.h"
#include "../include/app.h"

static window_t *shell_window = NULL;
static int gui_mode_active = 0;

void shell_gui_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Shell GUI mode...\n");

    shell_window = wm_create_window("torOS Shell", 50, 50, 700, 500, WS_OVERLAPPED);
    if (shell_window) {
        wm_show_window(shell_window);
        gui_mode_active = 1;
        printk_color(TERM_GREEN, "[BOOT] Shell GUI ready\n");
    } else {
        printk_color(TERM_RED, "[BOOT] Shell GUI failed\n");
    }
}

void shell_gui_draw(void)
{
    if (!gui_mode_active || !shell_window || !shell_window->framebuffer)
        return;

    uint32 *fb = shell_window->framebuffer;
    int w = shell_window->width;
    int h = shell_window->height;

    /* Dark terminal-like background */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            fb[y * w + x] = 0xFF1A1A2E;

    /* Title bar area */
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < w; x++)
            fb[y * w + x] = 0xFF0078D7;

    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 6, "torOS Shell v0.4");

    /* Status line */
    fb_set_color(0xFF00FF00);
    fb_draw_string(8, 40, "GUI Shell ready. Type 'help' for commands.");
    fb_draw_string(8, 60, "Available commands: apps, calc, editor, paint, browser, settings");

    /* Prompt */
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, h - 40, "$ ");
}

void shell_gui_handle_input(const char *cmd)
{
    if (!cmd || !*cmd) return;

    if (strcmp(cmd, "apps") == 0) {
        app_manager_list();
    } else if (strcmp(cmd, "calc") == 0) {
        app_launch("calculator");
    } else if (strcmp(cmd, "editor") == 0) {
        app_launch("editor");
    } else if (strcmp(cmd, "paint") == 0) {
        app_launch("paint");
    } else if (strcmp(cmd, "browser") == 0) {
        app_launch("browser");
    } else if (strcmp(cmd, "settings") == 0) {
        app_launch("settings");
    } else if (strcmp(cmd, "help") == 0) {
        printk_color(TERM_CYAN, "\n=== GUI Shell Commands ===\n");
        printk("  apps     - List available applications\n");
        printk("  calc     - Launch calculator\n");
        printk("  editor   - Launch text editor\n");
        printk("  paint    - Launch paint application\n");
        printk("  browser  - Launch web browser\n");
        printk("  settings - Launch settings panel\n");
        printk("  exit     - Return to text shell\n");
        printk("\n");
    } else if (strcmp(cmd, "exit") == 0) {
        gui_mode_active = 0;
        if (shell_window) {
            wm_destroy_window(shell_window);
            shell_window = NULL;
        }
    } else {
        printk_color(TERM_YELLOW, "Unknown GUI command: %s\n", cmd);
    }
}

int shell_gui_is_active(void)
{
    return gui_mode_active;
}

void shell_gui_toggle(void)
{
    if (gui_mode_active)
        gui_mode_active = 0;
    else
        shell_gui_init();
}

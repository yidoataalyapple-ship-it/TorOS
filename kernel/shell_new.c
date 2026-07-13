/*
 * torOS Shell v0.4 - GUI Integration
 * Extended commands for new subsystems
 */

#include "../include/toros.h"
#include "../include/window.h"
#include "../include/widget.h"
#include "../include/gpu.h"

static void cmd_input(void) {
    input_dump_devices();
    printk_color(TERM_CYAN, "\nInput event buffer: %d events\n\n", input_get_global_event_count());
}

static void cmd_gpu(void) {
    printk_color(TERM_CYAN, "\n=== GPU Subsystem ===\n");
    printk("  Initialized: %s\n", gpu_is_initialized() ? "yes" : "no");
    display_mode_t info;
    gpu_get_display_info(&info);
    printk("  Display: %dx%d @ %d bpp\n", info.width, info.height, info.bpp);
    printk_color(TERM_GREEN, "  Features: VirtIO-GPU, DMA-BUF\n\n");
}

static void cmd_windows(void) {
    wm_dump_windows();
}

static void cmd_desktops(void) {
    printk_color(TERM_CYAN, "\n=== Virtual Desktops ===\n");
    printk("  Current: %d/%d\n", vd_get_current() + 1, vd_get_count());
    for (int i = 0; i < vd_get_count(); i++) {
        printk("  [%c] %s\n", (i == vd_get_current()) ? '*' : ' ', vd_get_name(i));
    }
    printk("\n");
}

static void cmd_buffer(void) {
    printk_color(TERM_CYAN, "\n=== GPU Buffer Manager ===\n");
    printk("  Mode: Double/Triple supported\n");
    printk("  VSync: configurable\n\n");
}

static void cmd_dmabuf(void) {
    extern void dmabuf_list_all(void);
    dmabuf_list_all();
}

static void cmd_cursor(void) {
    printk_color(TERM_CYAN, "\n=== Hardware Cursor ===\n");
    printk("  Visible: %s\n", hw_cursor_is_visible() ? "yes" : "no");
    uint32 cx, cy; hw_cursor_get_pos(&cx, &cy);
    printk("  Position: (%d, %d)\n\n", cx, cy);
}

static void cmd_vdtest(void) {
    printk_color(TERM_GREEN, "\n[TEST] Creating test windows...\n");
    window_t *w1 = wm_create_window("Test Window 1", 100, 100, 400, 300, WS_OVERLAPPED);
    window_t *w2 = wm_create_window("Test Window 2", 200, 150, 350, 250, WS_OVERLAPPED);
    if (w1) wm_show_window(w1);
    if (w2) wm_show_window(w2);
    wm_dump_windows();
}

static void cmd_widgtest(void) {
    printk_color(TERM_GREEN, "\n[TEST] Creating widgets...\n");
    widget_init();
    window_t *win = wm_create_window("Widget Test", 50, 50, 500, 400, WS_OVERLAPPED);
    if (win) {
        wm_show_window(win);
        btn_create(win, 20, 40, 100, 28, "OK");
        btn_create(win, 130, 40, 100, 28, "Cancel");
        lbl_create(win, 20, 80, 200, 20, "Label widget test");
        tb_create(win, 20, 110, 200, 24);
        pb_create(win, 20, 150, 200, 20);
        lv_create(win, 20, 180, 200, 150);
        cbx_create(win, 20, 340, 150, 24, "Check me");
        printk_color(TERM_GREEN, "[TEST] Widgets created\n\n");
    }
}

/* Extend shell command parser - call from existing parse_cmd */
void extended_shell_commands(char *cmd, char *args) {
    if (!strcmp(cmd, "input")) cmd_input();
    else if (!strcmp(cmd, "gpu")) cmd_gpu();
    else if (!strcmp(cmd, "windows")) cmd_windows();
    else if (!strcmp(cmd, "desktops")) cmd_desktops();
    else if (!strcmp(cmd, "buffer")) cmd_buffer();
    else if (!strcmp(cmd, "dmabuf")) cmd_dmabuf();
    else if (!strcmp(cmd, "cursor")) cmd_cursor();
    else if (!strcmp(cmd, "vdtest")) cmd_vdtest();
    else if (!strcmp(cmd, "widgtest")) cmd_widgtest();
    else if (!strcmp(cmd, "compose")) {
        extern void compositor_compose(void);
        compositor_compose();
        printk("Composed frame\n\n");
    }
    else {
        printk_color(TERM_RED, "Unknown: '%s'\n", cmd);
    }
    (void)args;
}

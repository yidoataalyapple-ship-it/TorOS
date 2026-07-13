/*
 * torOS Shell v0.4
 * Full desktop OS command interface
 */

#include "../include/toros.h"
#include "../include/window.h"
#include "../include/widget.h"
#include "../include/gpu.h"

#define MAX_CMD 128
static char cmd_buf[MAX_CMD];

static void print_prompt(void)
{
    printk_color(TERM_GREEN, "torOS");
    printk_color(TERM_WHITE, "::");
    printk_color(TERM_BLUE, "~");
    printk_color(TERM_WHITE, "$ ");
}

static void cmd_help(void)
{
    printk_color(TERM_CYAN, "\n=== TorOS v0.4.0 Shell Commands ===\n\n");
    printk_color(TERM_YELLOW, "  [Basic]\n");
    printk("    help clear uname free ps echo uptime colors\n");
    printk("    kmap mmu smp time\n\n");
    printk_color(TERM_YELLOW, "  [Filesystem]\n");
    printk("    ls stat cat touch rm write\n\n");
    printk_color(TERM_YELLOW, "  [User Programs]\n");
    printk("    userlist run\n\n");
    printk_color(TERM_YELLOW, "  [Graphics & GUI]\n");
    printk("    gfx gpu windows desktops vdtest widgtest compose cursor dmabuf buffer\n\n");
    printk_color(TERM_YELLOW, "  [Input]\n");
    printk("    input\n\n");
    printk_color(TERM_YELLOW, "  [System]\n");
    printk("    reboot halt\n\n");
}

static void cmd_clear(void) { uart_puts("\033[2J\033[H"); }

static void cmd_uname(void)
{
    printk_color(TERM_CYAN, "\n  TorOS v0.4.0 — Full Desktop Operating System\n");
    printk("  Architecture: ARM64 (AArch64)\n");
    printk("  CPU:          ARM Cortex-A72 x%d\n", smp_cpu_count());
    printk("  RAM:          2GB\n");
    printk("  Exception:    EL%d\n", (r_currentel() >> 2) & 3);
    printk("  Build:        " __DATE__ " " __TIME__ "\n");
    printk_color(TERM_GREEN, "\n  Subsystems:\n");
    printk("    Kernel:  MMU GICv3 SMP Scheduler Syscalls Spinlocks\n");
    printk("    Input:   VirtIO-Input USB-xHCI USB-HID\n");
    printk("    Video:   VirtIO-GPU Framebuffer Compositor WindowManager\n");
    printk("    Audio:   AC'97 HDA VirtIO-Sound Mixer PCM\n");
    printk("    Network: VirtIO-Net TCP/IP UDP Socket-API DNS HTTP\n");
    printk("    FS:      torFS VFS BlockDevice\n");
    printk("    Security: ASLR NX-BIT Sandboxing Audit\n");
    printk("    Init:    Services Drivers LoginManager\n");
    printk("\n");
}

static void cmd_free(void)
{
    usize f = get_free_pages();
    usize total = f * PAGE_SIZE;
    printk("\n  Free pages: %d (%d MB / %d KB)\n\n", 
           f, total / (1024 * 1024), total / 1024);
}

static void cmd_ps(void) { proc_table_dump(); printk("\n"); }
static void cmd_echo(char *a) { printk("%s\n", a ? a : ""); }

static void cmd_uptime(void)
{
    uint64 j = get_jiffies();
    uint64 hours = (j / 100 / 3600) % 24;
    uint64 mins = (j / 100 / 60) % 60;
    uint64 secs = (j / 100) % 60;
    printk("  Uptime: %02d:%02d:%02d\n\n", hours, mins, secs);
}

static void cmd_colors(void)
{
    printk("\n");
    printk_color(TERM_BLACK,  "  [0] Black   ");
    printk_color(TERM_RED,    "[1] Red     ");
    printk_color(TERM_GREEN,  "[2] Green   ");
    printk_color(TERM_YELLOW, "[3] Yellow\n");
    printk_color(TERM_BLUE,   "  [4] Blue    ");
    printk_color(TERM_MAGENTA,"[5] Magenta ");
    printk_color(TERM_CYAN,   "[6] Cyan    ");
    printk_color(TERM_WHITE,  "[7] White\n\n");
}

static void cmd_kmap(void)
{
    printk("\n  Kernel Memory Map:\n");
    printk("  .text   %p - %p\n", _text_start, _text_end);
    printk("  .rodata %p - %p\n", _rodata_start, _rodata_end);
    printk("  .data   %p - %p\n", _data_start, _data_end);
    printk("  .bss    %p - %p\n", _bss_start, _bss_end);
    printk("  _end    %p\n\n", _kernel_end);
}

static void cmd_mmu(void) 
{ 
    printk("\n  MMU: Enabled\n");
    printk("  Page size:   4KB\n");
    printk("  VA bits:     48-bit\n");
    printk("  Levels:      4-level page tables\n");
    printk("  Granule:     4KB\n");
    printk("  Caches:      Enabled (I-cache + D-cache)\n\n");
}

static void cmd_smp(void) { smp_dump(); }
static void cmd_time(void) { rtc_print_time(); printk("\n"); }
static void cmd_ls(void) { tfs_ls(); }
static void cmd_stat(void) { tfs_stat(); }

static void cmd_cat(char *a)
{
    if (!a || !*a) { printk("Usage: cat <file>\n"); return; }
    char buf[256]; int n = tfs_read(a, buf, sizeof(buf) - 1, 0);
    if (n < 0) { printk_color(TERM_RED, "Not found: %s\n", a); return; }
    buf[n] = '\0'; printk("\n%s\n\n", buf);
}

static void cmd_touch(char *a) { if (!a || !*a) { printk("Usage: touch <file>\n"); return; } tfs_create(a); }
static void cmd_rm(char *a) { if (!a || !*a) { printk("Usage: rm <file>\n"); return; } tfs_delete(a); }

static void cmd_write(char *a)
{
    if (!a || !*a) { printk("Usage: write <file> <text>\n"); return; }
    char *s = a; while (*s && *s != ' ') s++;
    if (!*s) { printk("Usage: write <file> <text>\n"); return; }
    *s = '\0'; s++; while (*s == ' ') s++;
    tfs_write(a, s, strlen(s), 0); printk("Written to '%s'\n", a);
}

static void cmd_userlist(void) { user_list(); }
static void cmd_run(char *a) { if (!a || !*a) { printk("Usage: run <prog>\n"); return; } user_run(a); }

static void cmd_gfx(void)
{
    if (!fb_is_initialized()) { printk("Framebuffer not available\n"); return; }
    fb_clear(0xFF1A1A2E);
    fb_set_color(0xFF00D4AA);
    fb_draw_border(8, 8, FB_WIDTH - 16, FB_HEIGHT - 16, 0xFF00D4AA);
    fb_draw_string(24, 24, "TorOS v0.4.0 Graphics Test");
    fb_draw_rect(50, 80, 200, 120, 0xFFFF3366);
    fb_draw_rect(300, 80, 200, 120, 0xFF33FF66);
    fb_draw_rect(550, 80, 200, 120, 0xFF3366FF);
    fb_draw_line(50, 250, 750, 250, 0xFFFFFFFF);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(100, 210, "R"); fb_draw_string(350, 210, "G"); fb_draw_string(600, 210, "B");
    fb_set_color(0xFFAAAAAA);
    fb_draw_string(200, 300, "Framebuffer: 1024x768 @ 32bpp");
    fb_draw_string(220, 320, "MMU GICv3 SMP Scheduler Active");
    printk("Graphics test complete\n\n");
}

/* ===== v0.4 GUI Subsystem Commands ===== */

static void cmd_input(void) {
    extern void input_dump_devices(void);
    extern int input_get_global_event_count(void);
    input_dump_devices();
    printk_color(TERM_CYAN, "\nInput event buffer: %d events\n\n", input_get_global_event_count());
}

static void cmd_gpu(void) {
    printk_color(TERM_CYAN, "\n=== GPU Subsystem ===\n");
    printk("  Initialized: %s\n", gpu_is_initialized() ? "yes" : "no");
    display_mode_t info;
    gpu_get_display_info(&info);
    printk("  Display: %dx%d @ %d bpp\n", info.width, info.height, info.bpp);
    printk_color(TERM_GREEN, "  Features: VirtIO-GPU, DMA-BUF, Hardware Cursor\n\n");
}

static void cmd_windows(void) {
    extern void wm_dump_windows(void);
    wm_dump_windows();
}

static void cmd_desktops(void) {
    extern int vd_get_current(void);
    extern int vd_get_count(void);
    extern const char *vd_get_name(int);
    printk_color(TERM_CYAN, "\n=== Virtual Desktops ===\n");
    printk("  Current: %d/%d\n", vd_get_current() + 1, vd_get_count());
    for (int i = 0; i < vd_get_count(); i++) {
        printk("  [%c] %s\n", (i == vd_get_current()) ? '*' : ' ', vd_get_name(i));
    }
    printk("\n");
}

static void cmd_buffer(void) {
    printk_color(TERM_CYAN, "\n=== GPU Buffer Manager ===\n");
    printk("  Mode: Double/Triple buffering supported\n");
    printk("  VSync: 60Hz timer-based\n\n");
}

static void cmd_dmabuf(void) {
    extern void dmabuf_list_all(void);
    dmabuf_list_all();
}

static void cmd_cursor(void) {
    extern int hw_cursor_is_visible(void);
    extern void hw_cursor_get_pos(uint32 *, uint32 *);
    printk_color(TERM_CYAN, "\n=== Hardware Cursor ===\n");
    printk("  Visible: %s\n", hw_cursor_is_visible() ? "yes" : "no");
    uint32 cx, cy; hw_cursor_get_pos(&cx, &cy);
    printk("  Position: (%d, %d)\n\n", cx, cy);
}

static void cmd_vdtest(void) {
    printk_color(TERM_GREEN, "\n[TEST] Creating test windows...\n");
    extern window_t *wm_create_window(const char *, int, int, int, int, uint32);
    extern void wm_show_window(window_t *);
    extern void wm_dump_windows(void);
    window_t *w1 = wm_create_window("Test Window 1", 100, 100, 400, 300, 0x10);
    window_t *w2 = wm_create_window("Test Window 2", 200, 150, 350, 250, 0x10);
    if (w1) wm_show_window(w1);
    if (w2) wm_show_window(w2);
    wm_dump_windows();
}

static void cmd_widgtest(void) {
    printk_color(TERM_GREEN, "\n[TEST] Creating widgets...\n");
    extern void widget_init(void);
    extern void *btn_create(void *, int, int, int, int, const char *);
    extern void *lbl_create(void *, int, int, int, int, const char *);
    extern void *tb_create(void *, int, int, int, int);
    extern void *pb_create(void *, int, int, int, int);
    extern void *lv_create(void *, int, int, int, int);
    extern void *cbx_create(void *, int, int, int, int, const char *);
    extern window_t *wm_create_window(const char *, int, int, int, int, uint32);
    extern void wm_show_window(window_t *);
    widget_init();
    window_t *win = wm_create_window("Widget Test", 50, 50, 500, 400, 0x10);
    if (win) {
        wm_show_window(win);
        btn_create(win, 20, 40, 100, 28, "OK");
        btn_create(win, 130, 40, 100, 28, "Cancel");
        lbl_create(win, 20, 80, 200, 20, "Label widget test");
        tb_create(win, 20, 110, 200, 24);
        pb_create(win, 20, 150, 200, 20);
        lv_create(win, 20, 180, 200, 150);
        cbx_create(win, 20, 340, 150, 24, "Check me");
        printk_color(TERM_GREEN, "[TEST] Widgets created successfully\n\n");
    }
}

static void cmd_compose(void) {
    extern void compositor_compose(void);
    compositor_compose();
    printk("Compositor frame rendered\n\n");
}

static void cmd_reboot(void) { printk("\nRebooting...\n"); __asm__("mov x0, #0x84000009; hvc #0"); while (1) wfi(); }
static void cmd_halt(void) { printk("\nSystem halted.\n"); while (1) wfi(); }

static void parse_cmd(char *cmd)
{
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;
    char *args = cmd; while (*args && *args != ' ') args++;
    if (*args) { *args = '\0'; args++; while (*args == ' ') args++; } else args = NULL;

    if (!strcmp(cmd, "help"))         cmd_help();
    else if (!strcmp(cmd, "clear"))   cmd_clear();
    else if (!strcmp(cmd, "uname"))   cmd_uname();
    else if (!strcmp(cmd, "free"))    cmd_free();
    else if (!strcmp(cmd, "ps"))      cmd_ps();
    else if (!strcmp(cmd, "echo"))    cmd_echo(args);
    else if (!strcmp(cmd, "uptime"))  cmd_uptime();
    else if (!strcmp(cmd, "colors"))  cmd_colors();
    else if (!strcmp(cmd, "kmap"))    cmd_kmap();
    else if (!strcmp(cmd, "mmu"))     cmd_mmu();
    else if (!strcmp(cmd, "smp"))     cmd_smp();
    else if (!strcmp(cmd, "time"))    cmd_time();
    else if (!strcmp(cmd, "ls"))      cmd_ls();
    else if (!strcmp(cmd, "stat"))    cmd_stat();
    else if (!strcmp(cmd, "cat"))     cmd_cat(args);
    else if (!strcmp(cmd, "touch"))   cmd_touch(args);
    else if (!strcmp(cmd, "rm"))      cmd_rm(args);
    else if (!strcmp(cmd, "write"))   cmd_write(args);
    else if (!strcmp(cmd, "userlist")) cmd_userlist();
    else if (!strcmp(cmd, "run"))     cmd_run(args);
    else if (!strcmp(cmd, "gfx"))     cmd_gfx();
    /* v0.4 GUI commands */
    else if (!strcmp(cmd, "input"))   cmd_input();
    else if (!strcmp(cmd, "gpu"))     cmd_gpu();
    else if (!strcmp(cmd, "windows")) cmd_windows();
    else if (!strcmp(cmd, "desktops")) cmd_desktops();
    else if (!strcmp(cmd, "buffer"))  cmd_buffer();
    else if (!strcmp(cmd, "dmabuf"))  cmd_dmabuf();
    else if (!strcmp(cmd, "cursor"))  cmd_cursor();
    else if (!strcmp(cmd, "vdtest"))  cmd_vdtest();
    else if (!strcmp(cmd, "widgtest")) cmd_widgtest();
    else if (!strcmp(cmd, "compose")) cmd_compose();
    else if (!strcmp(cmd, "reboot"))  cmd_reboot();
    else if (!strcmp(cmd, "halt"))    cmd_halt();
    else printk_color(TERM_RED, "Unknown command: '%s'. Type 'help' for list.\n", cmd);
}

void shell_run(void)
{
    char c;
    printk_color(TERM_GREEN, "\n========================================\n");
    printk_color(TERM_GREEN, "  TorOS Shell v0.4\n");
    printk_color(TERM_GREEN, "  Type 'help' for all commands\n");
    printk_color(TERM_GREEN, "========================================\n\n");
    while (1) {
        print_prompt();
        int len = 0;
        while (1) {
            while (!uart_getc(&c)) wfi();
            if (c == '\r' || c == '\n') { uart_putc('\n'); cmd_buf[len] = '\0'; parse_cmd(cmd_buf); break; }
            else if ((c == '\b' || c == 0x7F) && len > 0) { len--; uart_puts("\b \b"); }
            else if (c == 0x03) { uart_putc('\n'); break; }
            else if (len < MAX_CMD - 1 && c >= 0x20 && c < 0x7F) { cmd_buf[len++] = c; uart_putc(c); }
        }
    }
}

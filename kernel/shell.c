/*
 * torOS Shell v0.3
 */

#include "../include/toros.h"

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
    printk_color(TERM_CYAN, "\n=== torOS v0.3 Commands ===\n\n");
    printk("  help clear uname free ps echo uptime colors kmap mmu\n");
    printk("  smp time ls stat cat touch rm write\n");
    printk("  userlist run gfx reboot halt\n\n");
}

static void cmd_clear(void) { uart_puts("\033[2J\033[H"); }

static void cmd_uname(void)
{
    printk_color(TERM_CYAN, "\n  torOS v0.3.0\n");
    printk("  ARM64 | Cortex-A72 x%d | 2GB | EL%d\n", smp_cpu_count(), (r_currentel() >> 2) & 3);
    printk("  MMU GICv3 SMP torFS FB SCHED VM SPINLOCK\n");
    printk("  " __DATE__ " " __TIME__ "\n\n");
}

static void cmd_free(void)
{
    usize f = get_free_pages();
    printk("\n  Free: %d pages (%d MB)\n\n", f, (f * PAGE_SIZE) / (1024 * 1024));
}

static void cmd_ps(void) { proc_table_dump(); printk("\n"); }
static void cmd_echo(char *a) { printk("%s\n", a ? a : ""); }

static void cmd_uptime(void)
{
    uint64 j = get_jiffies();
    printk("  %d:%02d:%02d\n\n", (j / 100 / 3600) % 24, (j / 100 / 60) % 60, (j / 100) % 60);
}

static void cmd_colors(void)
{
    printk("\n");
    printk_color(TERM_BLACK, "  Black "); printk_color(TERM_RED, "Red ");
    printk_color(TERM_GREEN, "Green "); printk_color(TERM_YELLOW, "Yellow\n");
    printk_color(TERM_BLUE, "  Blue "); printk_color(TERM_MAGENTA, "Magenta ");
    printk_color(TERM_CYAN, "Cyan "); printk_color(TERM_WHITE, "White\n\n");
}

static void cmd_kmap(void)
{
    printk("\n  .text %p-%p\n  .rodata %p-%p\n  .data %p-%p\n  .bss %p-%p\n  kernel %p\n\n",
           _text_start, _text_end, _rodata_start, _rodata_end,
           _data_start, _data_end, _bss_start, _bss_end, _kernel_end);
}

static void cmd_mmu(void) { printk("\n  MMU: ON, 4KB, 48-bit, 4 levels\n\n"); }
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
    if (!fb_is_initialized()) { printk("FB not available\n"); return; }
    fb_clear(0xFF1A1A2E);
    fb_set_color(0xFF00D4AA);
    fb_draw_border(8, 8, FB_WIDTH - 16, FB_HEIGHT - 16, 0xFF00D4AA);
    fb_draw_string(24, 24, "torOS v0.3 Graphics");
    fb_draw_rect(50, 80, 200, 120, 0xFFFF3366);
    fb_draw_rect(300, 80, 200, 120, 0xFF33FF66);
    fb_draw_rect(550, 80, 200, 120, 0xFF3366FF);
    fb_draw_line(50, 250, 750, 250, 0xFFFFFFFF);
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(100, 210, "R"); fb_draw_string(350, 210, "G"); fb_draw_string(600, 210, "B");
    fb_draw_string(250, 300, "torOS v0.3 Framebuffer");
    printk("GFX done\n\n");
}

static void cmd_reboot(void) { printk("\nRebooting...\n"); __asm__("mov x0, #0x84000009; hvc #0"); while (1) wfi(); }
static void cmd_halt(void) { printk("\nHalted.\n"); while (1) wfi(); }

static void parse_cmd(char *cmd)
{
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;
    char *args = cmd; while (*args && *args != ' ') args++;
    if (*args) { *args = '\0'; args++; while (*args == ' ') args++; } else args = NULL;

    if (!strcmp(cmd, "help"))       cmd_help();
    else if (!strcmp(cmd, "clear")) cmd_clear();
    else if (!strcmp(cmd, "uname")) cmd_uname();
    else if (!strcmp(cmd, "free"))  cmd_free();
    else if (!strcmp(cmd, "ps"))    cmd_ps();
    else if (!strcmp(cmd, "echo"))  cmd_echo(args);
    else if (!strcmp(cmd, "uptime")) cmd_uptime();
    else if (!strcmp(cmd, "colors")) cmd_colors();
    else if (!strcmp(cmd, "kmap"))  cmd_kmap();
    else if (!strcmp(cmd, "mmu"))   cmd_mmu();
    else if (!strcmp(cmd, "smp"))   cmd_smp();
    else if (!strcmp(cmd, "time"))  cmd_time();
    else if (!strcmp(cmd, "ls"))    cmd_ls();
    else if (!strcmp(cmd, "stat"))  cmd_stat();
    else if (!strcmp(cmd, "cat"))   cmd_cat(args);
    else if (!strcmp(cmd, "touch")) cmd_touch(args);
    else if (!strcmp(cmd, "rm"))    cmd_rm(args);
    else if (!strcmp(cmd, "write")) cmd_write(args);
    else if (!strcmp(cmd, "userlist")) cmd_userlist();
    else if (!strcmp(cmd, "run"))   cmd_run(args);
    else if (!strcmp(cmd, "gfx"))   cmd_gfx();
    else if (!strcmp(cmd, "reboot")) cmd_reboot();
    else if (!strcmp(cmd, "halt"))  cmd_halt();
    else printk_color(TERM_RED, "Unknown: '%s'\n", cmd);
}

void shell_run(void)
{
    char c;
    printk_color(TERM_GREEN, "torOS Shell v0.3\n");
    printk("Type 'help' for commands.\n\n");
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

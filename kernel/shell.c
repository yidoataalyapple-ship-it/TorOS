/*
 * torOS Shell v0.2
 * Interactive command interpreter with user programs
 */

#include "../include/toros.h"

#define MAX_CMD  128
#define MAX_ARGS 16

static char cmd_buf[MAX_CMD];
static int cmd_len = 0;

static void print_prompt(void)
{
    printk_color(TERM_GREEN, "torOS");
    printk_color(TERM_WHITE, ":");
    printk_color(TERM_BLUE, "~");
    printk_color(TERM_WHITE, "$ ");
}

static void cmd_help(void)
{
    printk_color(TERM_CYAN, "\n=== torOS Shell Commands ===\n\n");
    printk("  %-12s - Show this help\n", "help");
    printk("  %-12s - Clear screen\n", "clear");
    printk("  %-12s - Show system info\n", "uname");
    printk("  %-12s - Show memory status\n", "free");
    printk("  %-12s - List processes\n", "ps");
    printk("  %-12s - Echo text\n", "echo <text>");
    printk("  %-12s - Show uptime\n", "uptime");
    printk("  %-12s - Test printk colors\n", "colors");
    printk("  %-12s - Show kernel memory map\n", "kmap");
    printk("  %-12s - Show MMU status\n", "mmu");
    printk("  %-12s - List user programs\n", "userlist");
    printk("  %-12s - Run a user program\n", "run <prog>");
    printk("  %-12s - Graphics test\n", "gfx");
    printk("  %-12s - Reboot system\n", "reboot");
    printk("  %-12s - Shutdown\n", "halt");
    printk("\n");
}

static void cmd_clear(void)  { uart_puts("\033[2J\033[H"); }

static void cmd_uname(void)
{
    printk_color(TERM_CYAN, "\n  System:   torOS\n");
    printk_color(TERM_WHITE, "  Version:  0.2.0-alpha\n");
    printk_color(TERM_WHITE, "  Arch:     AArch64 (ARM64)\n");
    printk_color(TERM_WHITE, "  CPU:      Cortex-A72\n");
    printk_color(TERM_WHITE, "  Cores:    4\n");
    printk_color(TERM_WHITE, "  RAM:      2GB\n");
    printk_color(TERM_WHITE, "  Features: MMU, GICv3, Framebuffer, Scheduler\n");
    printk_color(TERM_WHITE, "  Build:    " __DATE__ " " __TIME__ "\n\n");
}

static void cmd_free(void)
{
    usize free = get_free_pages();
    usize total = (2048 * 1024 * 1024) / PAGE_SIZE;
    usize used = total - free;
    printk_color(TERM_CYAN, "\n  Memory Status:\n");
    printk("  Total: %d pages (%d MB)\n", total, 2048);
    printk_color(TERM_GREEN, "  Free:  %d pages (%d MB)\n", free, (free * PAGE_SIZE) / (1024 * 1024));
    printk_color(TERM_YELLOW, "  Used:  %d pages (%d MB)\n\n", used, (used * PAGE_SIZE) / (1024 * 1024));
}

static void cmd_ps(void)     { proc_table_dump(); printk("\n"); }
static void cmd_echo(char *a) { if (a && *a) printk("%s\n", a); else printk("\n"); }

static void cmd_uptime(void)
{
    uint64 j = get_jiffies();
    uint64 sec = j / 100, min = sec / 60, hr = min / 60;
    printk_color(TERM_CYAN, "  Uptime: %d:%02d:%02d (jiffies: %d)\n\n",
                 hr % 24, min % 60, sec % 60, j);
}

static void cmd_colors(void)
{
    printk("\n");
    printk_color(TERM_BLACK,   "  [0] Black  ");  printk_color(TERM_RED,     "  [1] Red\n");
    printk_color(TERM_GREEN,   "  [2] Green  ");  printk_color(TERM_YELLOW,  "  [3] Yellow\n");
    printk_color(TERM_BLUE,    "  [4] Blue   ");  printk_color(TERM_MAGENTA, "  [5] Magenta\n");
    printk_color(TERM_CYAN,    "  [6] Cyan   ");  printk_color(TERM_WHITE,   "  [7] White\n\n");
}

static void cmd_kmap(void)
{
    printk_color(TERM_CYAN, "\n  Kernel Memory Map:\n");
    printk("  .text   %p - %p\n", _text_start, _text_end);
    printk("  .rodata %p - %p\n", _rodata_start, _rodata_end);
    printk("  .data   %p - %p\n", _data_start, _data_end);
    printk("  .bss    %p - %p\n", _bss_start, _bss_end);
    printk("  Kernel:  %p - %p\n\n", _text_start, _kernel_end);
}

static void cmd_mmu(void)
{
    printk_color(TERM_CYAN, "\n  MMU Status:\n");
    printk_color(TERM_GREEN, "  Status:  ENABLED\n");
    printk("  Granule: 4KB\n");
    printk("  VA bits: 48\n");
    printk("  Levels:  4 (PGD > PUD > PMD > PTE)\n");
    printk("  Cache:   Write-Back, Inner Shareable\n\n");
}

static void cmd_gfx(void)
{
    if (!fb_is_initialized()) {
        printk_color(TERM_RED, "  Framebuffer not available!\n\n");
        return;
    }
    printk_color(TERM_CYAN, "\n  Drawing graphics test...\n\n");
    
    fb_clear(0xFF1A1A2E);
    fb_set_color(0xFF00D4AA);
    fb_draw_border(8, 8, FB_WIDTH - 16, FB_HEIGHT - 16, 0xFF00D4AA);
    fb_draw_string(24, 24, "torOS Graphics Test");
    
    fb_draw_rect(50, 100, 200, 150, 0xFFFF3366);
    fb_draw_rect(300, 100, 200, 150, 0xFF33FF66);
    fb_draw_rect(550, 100, 200, 150, 0xFF3366FF);
    
    fb_draw_line(50, 300, 750, 300, 0xFFFFFFFF);
    fb_draw_line(50, 320, 750, 400, 0xFFFFFF00);
    fb_draw_line(750, 320, 50, 400, 0xFFFF00FF);
    
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(100, 265, "Red");
    fb_draw_string(350, 265, "Green");
    fb_draw_string(600, 265, "Blue");
    fb_draw_string(250, 420, "torOS Framebuffer - Lines & Rectangles");
    
    printk_color(TERM_GREEN, "  Graphics test complete!\n\n");
}

static void cmd_userlist(void) { user_list(); }
static void cmd_run(char *a)
{
    if (a && *a) user_run(a);
    else { printk_color(TERM_RED, "  Usage: run <program>\n"); printk("  Use 'userlist' to see programs.\n"); }
}

static void cmd_reboot(void)
{
    printk_color(TERM_YELLOW, "\n  Rebooting...\n");
    __asm__ volatile("mov x0, #0x84000009; hvc #0");
    while (1) wfi();
}

static void cmd_halt(void)
{
    printk_color(TERM_YELLOW, "\n  System halted.\n");
    while (1) wfi();
}

static void parse_cmd(char *cmd)
{
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    char *args = cmd;
    while (*args && *args != ' ') args++;
    if (*args) { *args = '\0'; args++; while (*args == ' ') args++; } 
    else args = NULL;

    if (strcmp(cmd, "help") == 0)       cmd_help();
    else if (strcmp(cmd, "clear") == 0) cmd_clear();
    else if (strcmp(cmd, "uname") == 0) cmd_uname();
    else if (strcmp(cmd, "free") == 0)  cmd_free();
    else if (strcmp(cmd, "ps") == 0)    cmd_ps();
    else if (strcmp(cmd, "echo") == 0)  cmd_echo(args);
    else if (strcmp(cmd, "uptime") == 0) cmd_uptime();
    else if (strcmp(cmd, "colors") == 0) cmd_colors();
    else if (strcmp(cmd, "kmap") == 0)  cmd_kmap();
    else if (strcmp(cmd, "mmu") == 0)   cmd_mmu();
    else if (strcmp(cmd, "gfx") == 0)   cmd_gfx();
    else if (strcmp(cmd, "userlist") == 0) cmd_userlist();
    else if (strcmp(cmd, "run") == 0)   cmd_run(args);
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd, "halt") == 0)  cmd_halt();
    else {
        printk_color(TERM_RED, "  Unknown: '%s'\n", cmd);
        printk_color(TERM_YELLOW, "  Type 'help' for commands.\n");
    }
}

void shell_run(void)
{
    char c;
    printk_color(TERM_GREEN, "Welcome to torOS Shell v0.2!\n");
    printk_color(TERM_YELLOW, "Type 'help' for available commands.\n\n");

    while (1) {
        print_prompt();
        cmd_len = 0;
        while (1) {
            while (!uart_getc(&c)) wfi();
            if (c == '\r' || c == '\n') {
                uart_putc('\n');
                cmd_buf[cmd_len] = '\0';
                parse_cmd(cmd_buf);
                break;
            } else if ((c == '\b' || c == 0x7F) && cmd_len > 0) {
                cmd_len--; uart_puts("\b \b");
            } else if (c == 0x03) {
                uart_putc('\n'); break;
            } else if (cmd_len < MAX_CMD - 1 && c >= 0x20 && c < 0x7F) {
                cmd_buf[cmd_len++] = c; uart_putc(c);
            }
        }
    }
}

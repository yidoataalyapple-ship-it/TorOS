/*
 * torOS Shell
 * Interactive command interpreter
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
    printk_color(TERM_CYAN, "\n=== torOS Commands ===\n\n");
    printk("  %-12s - Show this help\n", "help");
    printk("  %-12s - Clear screen\n", "clear");
    printk("  %-12s - Show system info\n", "uname");
    printk("  %-12s - Show memory status\n", "free");
    printk("  %-12s - List processes\n", "ps");
    printk("  %-12s - Echo text\n", "echo");
    printk("  %-12s - Show uptime\n", "uptime");
    printk("  %-12s - Test printk colors\n", "colors");
    printk("  %-12s - Show kernel memory map\n", "kmap");
    printk("  %-12s - Reboot system\n", "reboot");
    printk("  %-12s - Shutdown\n", "halt");
    printk("\n");
}

static void cmd_clear(void)
{
    uart_puts("\033[2J\033[H");
}

static void cmd_uname(void)
{
    printk_color(TERM_CYAN, "\n  System:  ");
    printk_color(TERM_WHITE, "torOS\n");
    printk_color(TERM_CYAN, "  Version: ");
    printk_color(TERM_WHITE, "0.1.0-alpha\n");
    printk_color(TERM_CYAN, "  Arch:    ");
    printk_color(TERM_WHITE, "AArch64 (ARM64)\n");
    printk_color(TERM_CYAN, "  Kernel:  ");
    printk_color(TERM_WHITE, "torOS-KERNEL-1\n");
    printk_color(TERM_CYAN, "  Build:   ");
    printk_color(TERM_WHITE, __DATE__ " " __TIME__ "\n");
    printk_color(TERM_CYAN, "  License: ");
    printk_color(TERM_WHITE, "MIT\n\n");
}

static void cmd_free(void)
{
    usize free = get_free_pages();
    usize total = (2048 * 1024 * 1024) / PAGE_SIZE;
    usize used = total - free;
    printk_color(TERM_CYAN, "\n  Memory Status:\n");
    printk_color(TERM_WHITE, "  Total: %d pages (%d MB)\n", total, 2048);
    printk_color(TERM_GREEN, "  Free:  %d pages (%d MB)\n", free, (free * PAGE_SIZE) / (1024 * 1024));
    printk_color(TERM_YELLOW, "  Used:  %d pages (%d MB)\n\n", used, (used * PAGE_SIZE) / (1024 * 1024));
}

static void cmd_ps(void)
{
    proc_table_dump();
    printk("\n");
}

static void cmd_echo(char *args)
{
    if (args && *args)
        printk("%s\n", args);
    else
        printk("\n");
}

static void cmd_uptime(void)
{
    extern uint64 get_jiffies(void);
    uint64 j = get_jiffies();
    uint64 sec = j / 100;
    uint64 min = sec / 60;
    uint64 hr = min / 60;
    printk_color(TERM_CYAN, "  Uptime: %d:%02d:%02d (jiffies: %d)\n\n",
                 hr % 24, min % 60, sec % 60, j);
}

static void cmd_colors(void)
{
    printk("\n");
    printk_color(TERM_BLACK,   "  [0] Black  ");
    printk_color(TERM_RED,     "  [1] Red\n");
    printk_color(TERM_GREEN,   "  [2] Green  ");
    printk_color(TERM_YELLOW,  "  [3] Yellow\n");
    printk_color(TERM_BLUE,    "  [4] Blue   ");
    printk_color(TERM_MAGENTA, "  [5] Magenta\n");
    printk_color(TERM_CYAN,    "  [6] Cyan   ");
    printk_color(TERM_WHITE,   "  [7] White\n\n");
}

static void cmd_kmap(void)
{
    printk_color(TERM_CYAN, "\n  Kernel Memory Map:\n");
    printk_color(TERM_WHITE, "  .text   %p - %p  (code)\n", _text_start, _text_end);
    printk_color(TERM_WHITE, "  .rodata %p - %p  (ro data)\n", _rodata_start, _rodata_end);
    printk_color(TERM_WHITE, "  .data   %p - %p  (data)\n", _data_start, _data_end);
    printk_color(TERM_WHITE, "  .bss    %p - %p  (bss)\n", _bss_start, _bss_end);
    printk_color(TERM_WHITE, "  Kernel ends at: %p\n", _kernel_end);
    printk_color(TERM_WHITE, "  Stack top:      %p\n\n", _stack_top);
}

static void cmd_reboot(void)
{
    printk_color(TERM_YELLOW, "\n  Rebooting torOS...\n");
    /* PSCI reset */
    __asm__ volatile(
        "mov x0, #0x84000009\n"
        "hvc #0\n"
    );
    /* Fallback */
    while (1)
        wfi();
}

static void cmd_halt(void)
{
    printk_color(TERM_YELLOW, "\n  System halted.\n");
    while (1)
        wfi();
}

static void parse_cmd(char *cmd)
{
    while (*cmd == ' ')
        cmd++;
    if (*cmd == '\0')
        return;

    /* Find args */
    char *args = cmd;
    while (*args && *args != ' ')
        args++;
    if (*args) {
        *args = '\0';
        args++;
        while (*args == ' ')
            args++;
    } else {
        args = NULL;
    }

    /* Execute */
    if (strcmp(cmd, "help") == 0)       cmd_help();
    else if (strcmp(cmd, "clear") == 0) cmd_clear();
    else if (strcmp(cmd, "uname") == 0) cmd_uname();
    else if (strcmp(cmd, "free") == 0)  cmd_free();
    else if (strcmp(cmd, "ps") == 0)    cmd_ps();
    else if (strcmp(cmd, "echo") == 0)  cmd_echo(args);
    else if (strcmp(cmd, "uptime") == 0) cmd_uptime();
    else if (strcmp(cmd, "colors") == 0) cmd_colors();
    else if (strcmp(cmd, "kmap") == 0)  cmd_kmap();
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd, "halt") == 0)  cmd_halt();
    else {
        printk_color(TERM_RED, "  Unknown command: '%s'\n", cmd);
        printk_color(TERM_YELLOW, "  Type 'help' for available commands.\n");
    }
}

void shell_run(void)
{
    char c;

    printk_color(TERM_GREEN, "Welcome to torOS Shell!\n");
    printk_color(TERM_YELLOW, "Type 'help' for available commands.\n\n");

    while (1) {
        print_prompt();
        cmd_len = 0;

        while (1) {
            /* Poll for input */
            while (!uart_getc(&c))
                wfi();

            if (c == '\r' || c == '\n') {
                /* Enter pressed */
                uart_putc('\n');
                cmd_buf[cmd_len] = '\0';
                parse_cmd(cmd_buf);
                break;
            } else if (c == '\b' || c == 0x7F) {
                /* Backspace */
                if (cmd_len > 0) {
                    cmd_len--;
                    uart_puts("\b \b");
                }
            } else if (c == 0x03) {
                /* Ctrl-C */
                uart_putc('\n');
                break;
            } else if (cmd_len < MAX_CMD - 1 && c >= 0x20 && c < 0x7F) {
                /* Printable */
                cmd_buf[cmd_len++] = c;
                uart_putc(c);
            }
        }
    }
}

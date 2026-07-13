/*
 * torOS printk - Formatted output
 */

#include "../include/toros.h"

static char *digits = "0123456789ABCDEF";

static void print_hex(uint64 val, int width)
{
    char buf[17];
    int i;
    for (i = 15; i >= 0; i--) {
        buf[i] = digits[val & 0xF];
        val >>= 4;
    }
    buf[16] = '\0';
    if (width <= 0 || width > 16)
        width = 16;
    uart_puts(buf + 16 - width);
}

static void print_dec(int64 val)
{
    char buf[32];
    int i = 30;
    int neg = 0;
    buf[31] = '\0';

    if (val < 0) {
        neg = 1;
        val = -val;
    }

    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = digits[val % 10];
            val /= 10;
        }
    }

    if (neg)
        buf[i--] = '-';

    uart_puts(&buf[i + 1]);
}

static void print_udec(uint64 val)
{
    char buf[32];
    int i = 30;
    buf[31] = '\0';

    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = digits[val % 10];
            val /= 10;
        }
    }
    uart_puts(&buf[i + 1]);
}

static void print_ptr(void *p)
{
    uart_puts("0x");
    print_hex((uint64)p, 16);
}

static const char *colors[] = {
    "\033[30m",  /* black */
    "\033[31m",  /* red */
    "\033[32m",  /* green */
    "\033[33m",  /* yellow */
    "\033[34m",  /* blue */
    "\033[35m",  /* magenta */
    "\033[36m",  /* cyan */
    "\033[37m",  /* white */
};

void printk(const char *fmt, ...)
{
    uint64 *args = (uint64 *)&fmt + 1;
    int arg_idx = 0;

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
            case 'd':
                print_dec((int64)args[arg_idx++]);
                break;
            case 'u':
                print_udec(args[arg_idx++]);
                break;
            case 'x':
                uart_puts("0x");
                print_hex(args[arg_idx++], 16);
                break;
            case 'p':
                print_ptr((void *)args[arg_idx++]);
                break;
            case 's':
                uart_puts((const char *)args[arg_idx++]);
                break;
            case 'c':
                uart_putc((char)args[arg_idx++]);
                break;
            case '%':
                uart_putc('%');
                break;
            default:
                uart_putc('%');
                uart_putc(*fmt);
                break;
            }
        } else {
            uart_putc(*fmt);
        }
        fmt++;
    }
}

void printk_color(int color, const char *fmt, ...)
{
    if (color >= 0 && color < 8)
        uart_puts(colors[color]);
    uint64 *args = (uint64 *)&fmt + 1;
    printk(fmt, *args);
    uart_puts("\033[0m");
}

void print_logo(void)
{
    uart_puts("\033[36m");
    uart_puts("\n");
    uart_puts("    XXXXXXXXXXX XXXXXXXXXX XXXXXXXXXXX  XXXXXXXXXXX\n");
    uart_puts("   XXXXXXXX X   XX XX XX XXXXXXXXXXX X XXXXXXXXXX\n");
    uart_puts("   X   XXXX X   XXX XXX  XXX    X  X   XXXX X\n");
    uart_puts("       XXX     XXX      XXXXXXX       XXX    \n");
    uart_puts("       XXX     XXX      XXXXXXX       XXX    \n");
    uart_puts("       XXX     XXX      XXX  X        XXX    \n");
    uart_puts("       XXXXX   XXXXX    XXXXX         XXXXX   \n");
    uart_puts("       XXXXX   XXXXX    XXXXX         XXXXX    \n");
    uart_puts("\033[0m");
    uart_puts("\n");
    uart_puts("\033[32m");
    uart_puts("  =============================================\n");
    uart_puts("      torOS v0.1.0 - ARM64 (AArch64)\n");
    uart_puts("      Modern Operating System Kernel\n");
    uart_puts("      Built from scratch\n");
    uart_puts("  =============================================\n");
    uart_puts("\033[0m\n");
}

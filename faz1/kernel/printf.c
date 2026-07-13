/*
 * printf.c — Formatlama motoru + kprintf (UART)
 * Destek: %d %i %u %x %X %p %s %c, l/ll uzunluk, 0-pad genişlik, %%
 */
#include <toros/printf.h>
#include <toros/uart.h>
#include <toros/string.h>
#include <stdarg.h>

/* Karakter havuzuna yazan mini akış */
struct sink {
    char  *buf;
    size_t cap;
    size_t pos;
    int    to_uart;
};

static void sink_putc(struct sink *sk, char c)
{
    if (sk->to_uart) {
        uart_putc(c);
    } else if (sk->buf && sk->pos + 1 < sk->cap) {
        sk->buf[sk->pos] = c;
    }
    sk->pos++;
}

static void sink_puts(struct sink *sk, const char *s)
{
    while (*s)
        sink_putc(sk, *s++);
}

static void sink_num(struct sink *sk, u64 v, int base, int sign, int width, char pad, int left)
{
    char buf[32];
    int i = 0, neg = 0;

    if (sign && (s64)v < 0) {
        neg = 1;
        v = (u64)(-(s64)v);
    }
    if (v == 0)
        buf[i++] = '0';
    while (v) {
        u32 d = (u32)(v % (u64)base);
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        v /= (u64)base;
    }
    if (neg)
        buf[i++] = '-';

    int len = i;
    if (!left)
        for (int j = len; j < width; j++)
            sink_putc(sk, pad);
    while (i > 0)
        sink_putc(sk, buf[--i]);
    if (left)
        for (int j = len; j < width; j++)
            sink_putc(sk, ' ');
}

static void sink_str_w(struct sink *sk, const char *s, int width, int left)
{
    int len = (int)strlen(s);
    if (!left)
        for (int j = len; j < width; j++)
            sink_putc(sk, ' ');
    sink_puts(sk, s);
    if (left)
        for (int j = len; j < width; j++)
            sink_putc(sk, ' ');
}

void kvformat(struct sink *sk, const char *fmt, va_list ap);

void kvformat(struct sink *sk, const char *fmt, va_list ap)
{
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            sink_putc(sk, *p);
            continue;
        }
        p++;
        if (*p == '%') {
            sink_putc(sk, '%');
            continue;
        }

        int left = 0;
        if (*p == '-') { left = 1; p++; }

        char pad = ' ';
        int width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        int len = 0;
        if (*p == 'l') {
            len = 1;
            p++;
            if (*p == 'l') { len = 2; p++; }
        }

        switch (*p) {
        case 'd':
        case 'i': {
            s64 v = (len == 0) ? va_arg(ap, int) : va_arg(ap, long);
            sink_num(sk, (u64)v, 10, 1, width, pad, left);
            break;
        }
        case 'u': {
            u64 v = (len == 0) ? va_arg(ap, unsigned) : va_arg(ap, unsigned long);
            sink_num(sk, v, 10, 0, width, pad, left);
            break;
        }
        case 'x':
        case 'X': {
            u64 v = (len == 0) ? va_arg(ap, unsigned) : va_arg(ap, unsigned long);
            sink_num(sk, v, 16, 0, width, pad, left);
            break;
        }
        case 'p': {
            u64 v = (u64)va_arg(ap, void *);
            sink_puts(sk, "0x");
            sink_num(sk, v, 16, 0, 16, '0', 0);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            sink_str_w(sk, s ? s : "(null)", width, left);
            break;
        }
        case 'c':
            sink_putc(sk, (char)va_arg(ap, int));
            break;
        default:
            sink_putc(sk, '%');
            if (*p)
                sink_putc(sk, *p);
            else
                p--;
            break;
        }
        if (!*p)
            break;
    }
}

int kprintf(const char *fmt, ...)
{
    struct sink sk = { .buf = NULL, .cap = 0, .pos = 0, .to_uart = 1 };
    va_list ap;
    va_start(ap, fmt);
    kvformat(&sk, fmt, ap);
    va_end(ap);
    return (int)sk.pos;
}

int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    struct sink sk = { .buf = buf, .cap = cap, .pos = 0, .to_uart = 0 };
    kvformat(&sk, fmt, ap);
    if (buf && cap)
        buf[sk.pos < cap ? sk.pos : cap - 1] = 0;
    return (int)sk.pos;
}

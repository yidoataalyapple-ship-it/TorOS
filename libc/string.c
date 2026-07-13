/*
 * torOS String Library
 */

#include "../include/toros.h"

void *memcpy(void *dst, const void *src, usize n)
{
    uint8 *d = dst;
    const uint8 *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, usize n)
{
    uint8 *d = dst;
    while (n--)
        *d++ = (uint8)c;
    return dst;

}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(*s1) - (int)(*s2);
}

usize strlen(const char *s)
{
    usize n = 0;
    while (*s++)
        n++;
    return n;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != 0)
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, usize n)
{
    usize i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

int strncmp(const char *s1, const char *s2, usize n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(*s1) - (int)(*s2);
}

char *itoa(int64 val, char *buf, int base)
{
    char *p = buf;
    int neg = 0;
    int64 v = val;

    if (v < 0 && base == 10) {
        neg = 1;
        v = -v;
    }

    do {
        int digit = v % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        v /= base;
    } while (v > 0);

    if (neg)
        *p++ = '-';
    *p = '\0';

    /* Reverse */
    int len = p - buf;
    for (int i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return buf;
}

char *utoa(uint64 val, char *buf, int base)
{
    char *p = buf;
    uint64 v = val;

    do {
        int digit = v % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        v /= base;
    } while (v > 0);

    *p = '\0';

    int len = p - buf;
    for (int i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return buf;
}

/* ===== Extended libc: added for v0.4 subsystem support ===== */

void *memmove(void *dst, const void *src, usize n)
{
    uint8 *d = (uint8 *)dst;
    const uint8 *s = (const uint8 *)src;
    if (d == s || n == 0)
        return dst;
    if (d < s) {
        for (usize i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (usize i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strncat(char *dst, const char *src, usize n)
{
    char *d = dst + strlen(dst);
    usize i = 0;
    for (; i < n && src[i]; i++)
        d[i] = src[i];
    d[i] = '\0';
    return dst;
}

char *strstr(const char *haystack, const char *needle)
{
    usize nl = strlen(needle);
    if (nl == 0)
        return (char *)haystack;
    for (const char *p = haystack; *p; p++) {
        if (*p == *needle && strncmp(p, needle, nl) == 0)
            return (char *)p;
    }
    return NULL;
}

int atoi(const char *s)
{
    int v = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

double atof(const char *s)
{
    double val = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') {
            val += (*s++ - '0') * frac;
            frac *= 0.1;
        }
    }
    return neg ? -val : val;
}

int abs(int v)
{
    return v < 0 ? -v : v;
}

/* Minimal vsnprintf: %s %c %d %i %u %x %ld %lu %lx %p %% */
int vsnprintf(char *buf, usize size, const char *fmt, va_list ap)
{
    usize pos = 0;
    char tmp[32];

    if (size == 0)
        return 0;

    while (*fmt && pos < size - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        fmt++;
        int long_flag = 0;
        if (*fmt == 'l') { long_flag = 1; fmt++; }

        switch (*fmt) {
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            while (*str && pos < size - 1)
                buf[pos++] = *str++;
            break;
        }
        case 'c':
            buf[pos++] = (char)va_arg(ap, int);
            break;
        case 'd':
        case 'i': {
            int64 v = long_flag ? va_arg(ap, int64) : va_arg(ap, int);
            itoa(v, tmp, 10);
            for (char *t = tmp; *t && pos < size - 1; t++)
                buf[pos++] = *t;
            break;
        }
        case 'u': {
            uint64 v = long_flag ? va_arg(ap, uint64) : va_arg(ap, uint);
            utoa(v, tmp, 10);
            for (char *t = tmp; *t && pos < size - 1; t++)
                buf[pos++] = *t;
            break;
        }
        case 'x': {
            uint64 v = long_flag ? va_arg(ap, uint64) : va_arg(ap, uint);
            utoa(v, tmp, 16);
            for (char *t = tmp; *t && pos < size - 1; t++)
                buf[pos++] = *t;
            break;
        }
        case 'p': {
            utoa((uint64)(uintptr)va_arg(ap, void *), tmp, 16);
            if (pos < size - 2) { buf[pos++] = '0'; buf[pos++] = 'x'; }
            for (char *t = tmp; *t && pos < size - 1; t++)
                buf[pos++] = *t;
            break;
        }
        case '%':
            buf[pos++] = '%';
            break;
        default:
            buf[pos++] = '%';
            if (pos < size - 1)
                buf[pos++] = *fmt;
            break;
        }
        if (*fmt)
            fmt++;
    }
    buf[pos] = '\0';
    return (int)pos;
}

int snprintf(char *buf, usize size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

/* sinf: Taylor series with range reduction (adequate for audio tone gen) */
#define PI_F 3.14159265358979f
float sinf(float x)
{
    /* Reduce to [-pi, pi] */
    while (x > PI_F) x -= 2.0f * PI_F;
    while (x < -PI_F) x += 2.0f * PI_F;

    float x2 = x * x;
    float term = x;
    float sum = x;
    /* sin(x) = x - x^3/3! + x^5/5! - x^7/7! + x^9/9! */
    for (int i = 1; i <= 4; i++) {
        term *= -x2 / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}

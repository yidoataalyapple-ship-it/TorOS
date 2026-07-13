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

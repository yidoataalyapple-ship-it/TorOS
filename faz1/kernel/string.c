/*
 * string.c — Freestanding string/mem implementasyonu
 */
#include <toros/string.h>

void *memset(void *dst, int c, size_t n)
{
    u8 *d = dst;
    /* 8 baytlık bloklar halinde hızlı doldur */
    u64 v = (u8)c;
    v |= v << 8; v |= v << 16; v |= v << 32;
    while (n >= 8 && ((uintptr_t)d & 7) == 0) {
        *(u64 *)d = v;
        d += 8; n -= 8;
    }
    while (n--)
        *d++ = (u8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    while (n >= 8 && ((uintptr_t)d & 7) == 0 && ((uintptr_t)s & 7) == 0) {
        *(u64 *)d = *(const u64 *)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    if (d == s || n == 0)
        return dst;
    if (d < s)
        return memcpy(dst, src, n);
    d += n; s += n;
    while (n--)
        *--d = *--s;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const u8 *x = a, *y = b;
    while (n--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t i = 0;
    while (s[i])
        i++;
    return i;
}

size_t strnlen(const char *s, size_t max)
{
    size_t i = 0;
    while (i < max && s[i])
        i++;
    return i;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0)
        return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strchr(const char *s, int c)
{
    for (; *s; s++)
        if (*s == (char)c)
            return (char *)s;
    return (c == 0) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    for (; *s; s++)
        if (*s == (char)c)
            last = s;
    if (c == 0)
        return (char *)s;
    return (char *)last;
}

char *strstr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    if (nl == 0)
        return (char *)h;
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, nl) == 0)
            return (char *)h;
    return NULL;
}

int atoi(const char *s)
{
    int v = 0, neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

static char *strtok_save;

char *strtok(char *s, const char *delim)
{
    if (!s)
        s = strtok_save;
    if (!s)
        return NULL;
    /* öndeki ayraçları atla */
    while (*s && strchr(delim, *s))
        s++;
    if (!*s) {
        strtok_save = NULL;
        return NULL;
    }
    char *tok = s;
    while (*s && !strchr(delim, *s))
        s++;
    if (*s) {
        *s = 0;
        strtok_save = s + 1;
    } else {
        strtok_save = NULL;
    }
    return tok;
}

/*
 * printf.h — Minimal kernel printf (UART çıkışı)
 */
#ifndef TOROS_PRINTF_H
#define TOROS_PRINTF_H

#include <toros/types.h>

/* %d %i %u %x %X %p %s %c %ld %lu %lx %lld %llu %llx %% destekler */
int kprintf(const char *fmt, ...);

/* Renkli log yardımcıları (ANSI) */
#define KCLR_RESET   "\x1b[0m"
#define KCLR_RED     "\x1b[31m"
#define KCLR_GREEN   "\x1b[32m"
#define KCLR_YELLOW  "\x1b[33m"
#define KCLR_BLUE    "\x1b[34m"
#define KCLR_MAGENTA "\x1b[35m"
#define KCLR_CYAN    "\x1b[36m"
#define KCLR_GRAY    "\x1b[90m"

#define kinfo(...)  kprintf(KCLR_CYAN   "[*] " KCLR_RESET __VA_ARGS__)
#define kok(...)    kprintf(KCLR_GREEN  "[+] " KCLR_RESET __VA_ARGS__)
#define kwarn(...)  kprintf(KCLR_YELLOW "[!] " KCLR_RESET __VA_ARGS__)
#define kerr(...)   kprintf(KCLR_RED    "[-] " KCLR_RESET __VA_ARGS__)

#endif

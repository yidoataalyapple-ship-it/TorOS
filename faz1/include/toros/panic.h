/*
 * panic.h — Kernel panik / ölümcül hata
 */
#ifndef TOROS_PANIC_H
#define TOROS_PANIC_H

void panic(const char *fmt, ...) __attribute__((noreturn));
void halt_forever(void) __attribute__((noreturn));

#endif

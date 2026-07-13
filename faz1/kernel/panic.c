/*
 * panic.c — Kernel panik
 */
#include <toros/panic.h>
#include <toros/printf.h>
#include <toros/uart.h>
#include <toros/types.h>
#include <stdarg.h>

extern int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);

void halt_forever(void)
{
    irq_disable();
    for (;;)
        wfi();
}

void panic(const char *fmt, ...)
{
    char msg[512];

    irq_disable();

    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    kprintf("\n" KCLR_RED "========== KERNEL PANIC ==========\n" KCLR_RESET);
    kprintf("%s\n", msg);
    kprintf(KCLR_RED "==================================\n" KCLR_RESET);
    kprintf("Sistem durduruldu.\n");
    halt_forever();
}

/*
 * torOS System Call Handler
 */

#include "../include/toros.h"

void syscall_handler(uint64 num, uint64 a0, uint64 a1, uint64 a2)
{
    (void)a0; (void)a1; (void)a2;
    switch (num) {
    case SYS_WRITE:
        /* a0 = fd, a1 = buf, a2 = len */
        uart_puts((const char *)a1);
        break;
    case SYS_READ:
        /* Not implemented */
        break;
    case SYS_EXIT:
        printk_color(TERM_YELLOW, "[SYSCALL] exit(%d)\n", a0);
        break;
    case SYS_FORK:
        printk_color(TERM_YELLOW, "[SYSCALL] fork()\n");
        break;
    case SYS_SLEEP:
        /* a0 = ms */
        sleep(a0);
        break;
    case SYS_GETPID:
        printk_color(TERM_YELLOW, "[SYSCALL] getpid()\n");
        break;
    case SYS_EXEC:
        printk_color(TERM_YELLOW, "[SYSCALL] exec(%s)\n", (const char *)a0);
        break;
    default:
        printk_color(TERM_RED, "[SYSCALL] Unknown syscall: %d\n", num);
        break;
    }
}

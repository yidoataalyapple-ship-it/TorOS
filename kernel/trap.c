/*
 * torOS Trap/Interrupt Handler
 * ARM64 exceptions and timer
 */

#include "../include/toros.h"

/* AArch64 system registers */
#define SCTLR_M     (1 << 0)    /* MMU enable */
#define SCTLR_C     (1 << 2)    /* Cache enable */
#define SCTLR_I     (1 << 12)   /* Instruction cache */

/* Generic Timer */
#define CNTV_CTL_ENABLE  (1 << 0)
#define CNTV_CTL_IMASK   (1 << 1)
#define CNTV_CTL_ISTATUS (1 << 2)

#define TIMER_FREQ_HZ    100      /* 100 Hz timer */

static volatile uint64 jiffies = 0;

void trap_handle(void *regs)
{
    uint64 esr, far, elr;
    __asm__ volatile("mrs %0, ESR_EL1" : "=r"(esr));
    __asm__ volatile("mrs %0, FAR_EL1" : "=r"(far));
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));

    uint64 ec = (esr >> 26) & 0x3F;  /* Exception class */
    uint64 iss = esr & 0x1FFFFFF;    /* Instruction specific */

    switch (ec) {
    case 0x15:  /* SVC from AArch64 */
        syscall_handler(iss, 0, 0, 0);
        break;
    case 0x24:  /* Data abort */
        printk_color(TERM_RED, "[TRAP] Data abort at ELR=%p, FAR=%p, ESR=%x\n",
                     elr, far, esr);
        break;
    case 0x20:  /* Instruction abort */
        printk_color(TERM_RED, "[TRAP] Inst abort at ELR=%p, FAR=%p\n", elr, far);
        break;
    default:
        printk_color(TERM_RED, "[TRAP] EC=%d, ESR=%x, ELR=%p\n", ec, esr, elr);
        break;
    }
}

void irq_handle(void)
{
    jiffies++;
    timer_set_next();
    schedule();
}

void fiq_handle(void)
{
    printk_color(TERM_YELLOW, "[IRQ] FIQ received\n");
}

void error_handle(void)
{
    printk_color(TERM_RED, "[TRAP] Error exception!\n");
}

void trap_init(void)
{
    /* Set vector table */
    extern char vector_table[];
    __asm__ volatile("msr VBAR_EL1, %0" :: "r"(vector_table));

    /* Enable interrupts at CPU */
    __asm__ volatile("msr DAIFClr, #0xF");
}

void timer_init(void)
{
    uint64 freq;
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));

    uint64 interval = freq / TIMER_FREQ_HZ;
    __asm__ volatile("msr CNTV_TVAL_EL0, %0" :: "r"(interval));

    uint64 ctl = CNTV_CTL_ENABLE;
    __asm__ volatile("msr CNTV_CTL_EL0, %0" :: "r"(ctl));

    timer_set_next();
}

void timer_set_next(void)
{
    uint64 freq;
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));
    uint64 interval = freq / TIMER_FREQ_HZ;
    __asm__ volatile("msr CNTV_TVAL_EL0, %0" :: "r"(interval));
}

uint64 get_jiffies(void)
{
    return jiffies;
}

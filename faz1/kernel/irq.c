/*
 * irq.c — İstisna işleyicileri ve IRQ dispatch tablosu
 */
#include <toros/irq.h>
#include <toros/gic.h>
#include <toros/printf.h>
#include <toros/panic.h>
#include <toros/sched.h>

#define MAX_HANDLERS 256

static irq_handler_t handlers[MAX_HANDLERS];
static void *handler_args[MAX_HANDLERS];

void irq_register(u32 intid, irq_handler_t handler, void *arg)
{
    if (intid >= MAX_HANDLERS) {
        kwarn("irq_register: intid %u tablo dışı\n", intid);
        return;
    }
    handlers[intid] = handler;
    handler_args[intid] = arg;
}

static void dump_frame(struct trap_frame *tf)
{
    kprintf("  ELR=%p SPSR=%p\n", (void *)tf->elr, (void *)tf->spsr);
    for (int i = 0; i < 30; i += 2)
        kprintf("  x%02d=%016lx x%02d=%016lx\n", i, tf->x[i], i + 1, tf->x[i + 1]);
    kprintf("  x30=%016lx\n", tf->x[30]);
}

void sync_exception_handler(struct trap_frame *tf, u64 kind)
{
    u64 esr, far, elr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    elr = tf->elr;

    u64 ec = (esr >> 26) & 0x3f;
    u64 iss = esr & 0x1ffffff;

    kprintf("\n" KCLR_RED "!!!! SYNC EXCEPTION (kind=%lu) !!!!\n" KCLR_RESET, kind);
    kprintf("  ESR=%p EC=0x%02lx ISS=0x%06lx FAR=%p ELR=%p\n",
            (void *)esr, ec, iss, (void *)far, (void *)elr);
    dump_frame(tf);
    panic("Senkron istisna (EC=0x%02lx)", ec);
}

struct trap_frame *irq_exception_handler(struct trap_frame *tf)
{
    u64 iar = gic_read_iar1();
    u32 intid = (u32)(iar & 0x3FF);

    if (intid < 1020) {
        if (intid < MAX_HANDLERS && handlers[intid]) {
            handlers[intid](intid, handler_args[intid]);
        } else {
            kwarn("İşlenmemiş IRQ: %u\n", intid);
        }
        gic_write_eoir1(iar);
        /* Timer tick scheduler task değiştirmiş olabilir */
        tf = sched_tick(tf);
    } else {
        /* Spurious (1023) veya özel */
        if (intid != 1023)
            kwarn("Özel INTID: %u\n", intid);
    }
    return tf;
}

void fiq_exception_handler(struct trap_frame *tf, u64 kind)
{
    kprintf(KCLR_RED "FIQ (kind=%lu)\n" KCLR_RESET, kind);
    dump_frame(tf);
    panic("FIQ alındı");
}

void serror_exception_handler(struct trap_frame *tf, u64 kind)
{
    u64 esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    kprintf(KCLR_RED "SError (kind=%lu) ESR=%p\n" KCLR_RESET, kind, (void *)esr);
    dump_frame(tf);
    panic("SError alındı");
}

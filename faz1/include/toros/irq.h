/*
 * irq.h — İstisna çerçevesi ve IRQ dispatch
 */
#ifndef TOROS_IRQ_H
#define TOROS_IRQ_H

#include <toros/types.h>

/* vectors.S ile BİREBİR aynı sıra (toplam 272 bayt, 16 hizalı) */
struct trap_frame {
    u64 x[31];      /* x0..x30  */
    u64 elr;        /* ELR_EL1  */
    u64 spsr;       /* SPSR_EL1 */
    u64 pad;        /* 16 hizalama */
};

#define TRAP_FRAME_SIZE 272

typedef void (*irq_handler_t)(u32 intid, void *arg);

void irq_register(u32 intid, irq_handler_t handler, void *arg);

/* Vektör girişlerinden çağrılan C fonksiyonları */
void sync_exception_handler(struct trap_frame *tf, u64 kind);
struct trap_frame *irq_exception_handler(struct trap_frame *tf);
void fiq_exception_handler(struct trap_frame *tf, u64 kind);
void serror_exception_handler(struct trap_frame *tf, u64 kind);

#endif

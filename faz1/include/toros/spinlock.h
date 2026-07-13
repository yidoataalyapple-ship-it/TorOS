/*
 * spinlock.h — ARM64 LDAXR/STLXR tabanlı spinlock
 * (Planda "MEVCUT" sayılan temel bileşen)
 */
#ifndef TOROS_SPINLOCK_H
#define TOROS_SPINLOCK_H

#include <toros/types.h>

typedef struct {
    volatile u32 lock;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_init(spinlock_t *l)
{
    l->lock = 0;
}

static inline void spin_lock(spinlock_t *l)
{
    u32 val, status;
    __asm__ volatile(
        "1: ldaxr   %w0, [%2]\n"
        "   cbnz    %w0, 2f\n"
        "   mov     %w0, #1\n"
        "   stlxr   %w1, %w0, [%2]\n"
        "   cbnz    %w1, 1b\n"
        "   b       3f\n"
        "2: wfe\n"
        "   b       1b\n"
        "3:\n"
        : "=&r"(val), "=&r"(status) : "r"(&l->lock) : "memory", "cc");
}

static inline int spin_trylock(spinlock_t *l)
{
    u32 val, res;
    __asm__ volatile(
        "   ldaxr   %w0, [%2]\n"
        "   cbnz    %w0, 1f\n"
        "   mov     %w0, #1\n"
        "   stlxr   %w1, %w0, [%2]\n"
        "   cbnz    %w1, 1f\n"
        "   mov     %w1, #1\n"
        "   b       2f\n"
        "1: mov     %w1, #0\n"
        "   clrex\n"
        "2:\n"
        : "=&r"(val), "=&r"(res) : "r"(&l->lock) : "memory", "cc");
    return res;
}

static inline void spin_unlock(spinlock_t *l)
{
    __asm__ volatile(
        "   stlr    %w0, [%1]\n"
        "   sev\n"
        :: "r"(0), "r"(&l->lock) : "memory");
}

/* IRQ-güvenli varyantlar */
static inline u64 spin_lock_irqsave(spinlock_t *l)
{
    u64 flags = irq_save();
    spin_lock(l);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *l, u64 flags)
{
    spin_unlock(l);
    irq_restore(flags);
}

#endif

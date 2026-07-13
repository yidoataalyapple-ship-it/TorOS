/*
 * torOS Spinlock
 * ARM64 LL/SC based multi-core synchronization
 */

#include "../include/toros.h"

void spin_init(spinlock_t *lock)
{
    lock->locked = 0;
    lock->cpu = 0xFF;
}

void spin_lock(spinlock_t *lock)
{
    uint64 cpu = r_mpidr() & 0xFF;
    while (1) {
        uint32 val;
        __asm__ volatile(
            "1: ldaxr %w0, [%1]\n"
            "   cbnz %w0, 2f\n"
            "   stxr %w0, %w2, [%1]\n"
            "   cbnz %w0, 1b\n"
            "   b 3f\n"
            "2: wfe\n"
            "   b 1b\n"
            "3:"
            : "=&r"(val)
            : "r"(&lock->locked), "r"(1)
            : "memory"
        );
        if (val == 0) { lock->cpu = cpu; break; }
    }
}

void spin_unlock(spinlock_t *lock)
{
    lock->cpu = 0xFF;
    __asm__ volatile(
        "stlr wzr, [%0]\n"
        "sev\n"
        :: "r"(&lock->locked) : "memory"
    );
}

int spin_trylock(spinlock_t *lock)
{
    uint32 val;
    __asm__ volatile(
        "ldaxr %w0, [%1]\n"
        "cbnz %w0, 1f\n"
        "stxr %w0, %w2, [%1]\n"
        "cbz %w0, 1f\n"
        "mov %w0, #1\n"
        "1:"
        : "=&r"(val)
        : "r"(&lock->locked), "r"(1)
        : "memory"
    );
    if (val == 0) { lock->cpu = r_mpidr() & 0xFF; return 1; }
    return 0;
}

static volatile int push_off_nest[NPROC] = {0};

void push_off(void)
{
    int cpu = r_mpidr() & 0xFF;
    __asm__ volatile("msr DAIFSet, #0xF");
    push_off_nest[cpu]++;
}

void pop_off(void)
{
    int cpu = r_mpidr() & 0xFF;
    if (push_off_nest[cpu] <= 0) panic("pop_off: not pushed");
    if (--push_off_nest[cpu] == 0)
        __asm__ volatile("msr DAIFClr, #0xF");
}

/*
 * types.h — Temel tipler ve yardımcı makrolar
 * TorOS ARM64 kernel
 */
#ifndef TOROS_TYPES_H
#define TOROS_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long      u64;
typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long        s64;
typedef u64                size_t;
typedef s64                ssize_t;
typedef u64                uintptr_t;
typedef int                pid_t;

typedef enum { false = 0, true = 1 } bool;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((u64)(a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((u64)(a) - 1))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define BIT(n) (1UL << (n))

#define KB (1024UL)
#define MB (1024UL * 1024UL)

/* ------------------------------------------------------------------ */
/* Bariyerler ve CPU yardımcıları                                      */
/* ------------------------------------------------------------------ */
static inline void dsb_sy(void)  { __asm__ volatile("dsb sy"  ::: "memory"); }
static inline void dsb_ish(void) { __asm__ volatile("dsb ish" ::: "memory"); }
static inline void dsb_st(void)  { __asm__ volatile("dsb st"  ::: "memory"); }
static inline void isb(void)     { __asm__ volatile("isb"     ::: "memory"); }
static inline void nop(void)     { __asm__ volatile("nop"); }
static inline void wfe(void)     { __asm__ volatile("wfe"     ::: "memory"); }
static inline void sev(void)     { __asm__ volatile("sev"     ::: "memory"); }
static inline void wfi(void)     { __asm__ volatile("wfi"); }
static inline void yield_cpu(void) { __asm__ volatile("yield"); }

static inline u64 read_mpidr(void)
{
    u64 v;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(v));
    return v;
}

static inline u64 read_current_el(void)
{
    u64 v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return (v >> 2) & 3;
}

static inline void irq_enable(void)  { __asm__ volatile("msr daifclr, #2" ::: "memory"); }
static inline void irq_disable(void) { __asm__ volatile("msr daifset, #2" ::: "memory"); }

static inline u64 irq_save(void)
{
    u64 daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    irq_disable();
    return daif;
}

static inline void irq_restore(u64 daif)
{
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
}

/* ------------------------------------------------------------------ */
/* MMIO erişim yardımcıları                                            */
/* ------------------------------------------------------------------ */
static inline u8 mmio_read8(u64 addr)
{
    return *(volatile u8 *)addr;
}

static inline u16 mmio_read16(u64 addr)
{
    return *(volatile u16 *)addr;
}

static inline u32 mmio_read32(u64 addr)
{
    return *(volatile u32 *)addr;
}

static inline u64 mmio_read64(u64 addr)
{
    return *(volatile u64 *)addr;
}

static inline void mmio_write8(u64 addr, u8 v)
{
    *(volatile u8 *)addr = v;
}

static inline void mmio_write16(u64 addr, u16 v)
{
    *(volatile u16 *)addr = v;
}

static inline void mmio_write32(u64 addr, u32 v)
{
    *(volatile u32 *)addr = v;
}

static inline void mmio_write64(u64 addr, u64 v)
{
    *(volatile u64 *)addr = v;
}

#endif /* TOROS_TYPES_H */

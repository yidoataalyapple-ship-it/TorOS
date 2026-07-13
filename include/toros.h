/*
 * torOS Kernel Header v0.4
 * Full desktop OS feature set
 */

#ifndef _TOROS_H
#define _TOROS_H

/* ===== Basic types ===== */
typedef unsigned char uint8; typedef unsigned short uint16;
typedef unsigned int uint32; typedef unsigned long uint64;
typedef signed char int8; typedef signed short int16;
typedef signed int int32; typedef signed long int64;
typedef unsigned long usize; typedef unsigned long uintptr;
typedef unsigned int uint; typedef unsigned int bool;
#define true 1
#define false 0
#define NULL ((void*)0)

#define PAGE_SIZE 4096
#define KERNEL_BASE 0x40000000UL
#define RAM_SIZE (2048UL * 1024 * 1024)

/* ===== Terminal colors ===== */
#define TERM_BLACK 0
#define TERM_RED 1
#define TERM_GREEN 2
#define TERM_YELLOW 3
#define TERM_BLUE 4
#define TERM_MAGENTA 5
#define TERM_CYAN 6
#define TERM_WHITE 7

/* ===== Kernel symbols ===== */
extern char _text_start[], _text_end[], _rodata_start[], _rodata_end[];
extern char _data_start[], _data_end[], _bss_start[], _bss_end[];
extern char _kernel_end[], _stack_top[], _dtb_ptr[];

/* ===== Spinlock ===== */
typedef struct { volatile uint32 locked; uint32 cpu; } spinlock_t;
void spin_init(spinlock_t *l); void spin_lock(spinlock_t *l);
void spin_unlock(spinlock_t *l); int spin_trylock(spinlock_t *l);
void push_off(void); void pop_off(void);

/* ===== UART ===== */
void uart_init(void); void uart_putc(char c); void uart_puts(const char *s); int uart_getc(char *c);

/* ===== Printk ===== */
void printk(const char *fmt, ...); void printk_color(int color, const char *fmt, ...); void print_logo(void);

/* ===== Memory Manager ===== */
void mm_init(void); void *page_alloc(void); void page_free(void *p); usize get_free_pages(void);
void *kmalloc(usize s); void kfree(void *p);

/* ===== String / libc ===== */
void *memcpy(void *d, const void *s, usize n); void *memset(void *d, int c, usize n);
int strcmp(const char *s1, const char *s2); usize strlen(const char *s);
char *strcpy(char *d, const char *s); char *strncpy(char *d, const char *s, usize n);
int strncmp(const char *s1, const char *s2, usize n);
char *itoa(int64 v, char *b, int base); char *utoa(uint64 v, char *b, int base);

/* ===== Virtual Memory ===== */
void vm_init(void); int vm_user_map(uint64 va, uint64 pa, uint64 f);
void vm_user_unmap(uint64 va); uint64 vm_va2pa(uint64 va); void vm_flush_tlb(void);

/* ===== GICv3 ===== */
void gic_init(void); void gic_enable_irq(uint irq); void gic_disable_irq(uint irq);
void gic_set_priority(uint irq, uint8 p); void gic_set_target(uint irq, uint cpu);
uint gic_get_irq(void); void gic_eoi(uint irq);

/* ===== Trap / Timer ===== */
void trap_init(void); void timer_init(void); void timer_set_next(void);
void trap_handle(void *r); void irq_handle(void); void fiq_handle(void); void error_handle(void);
uint64 get_jiffies(void);

/* ===== Process / Scheduler ===== */
#define NPROC 64
#define NAMELEN 16
typedef enum { PROC_UNUSED, PROC_RUNNABLE, PROC_RUNNING, PROC_SLEEPING, PROC_ZOMBIE } proc_state_t;
typedef struct context { uint64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, fp, lr, sp; } context_t;
typedef struct proc { uint64 pid; proc_state_t state; char name[NAMELEN]; context_t ctx; uint64 *stack; uint64 sleep_until; struct proc *next; } proc_t;
void sched_init(void); void yield(void); void sleep(uint64 ms); void proc_table_dump(void);
proc_t *proc_create(const char *n, void (*e)(void)); void schedule(void);

/* ===== Context Switch (ASM) ===== */
void context_switch(context_t *o, context_t *n); void context_first_switch(context_t *n);
void kernel_to_user(uint64 s, uint64 e, uint64 a);

/* ===== SMP ===== */
void smp_init(void); void smp_secondary_start(void); void smp_send_ipi(uint t, uint i);
void smp_broadcast_ipi(uint i); int smp_cpu_count(void); void smp_dump(void);

/* ===== Syscalls ===== */
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_EXIT 3
#define SYS_FORK 4
#define SYS_SLEEP 5
#define SYS_GETPID 6
#define SYS_EXEC 7
void syscall_handler(uint64 n, uint64 a0, uint64 a1, uint64 a2);

/* ===== Framebuffer ===== */
#define FB_WIDTH 1024
#define FB_HEIGHT 768
void fb_init(void); void fb_clear(uint32 c); void fb_putpixel(int x, int y, uint32 c);
void fb_draw_char(int x, int y, char c, uint32 col); void fb_set_color(uint32 c);
void fb_draw_string(int x, int y, const char *s); void fb_draw_line(int x0, int y0, int x1, int y1, uint32 c);
void fb_draw_rect(int x, int y, int w, int h, uint32 c); void fb_draw_border(int x, int y, int w, int h, uint32 c);
int fb_is_initialized(void);

/* ===== User Programs ===== */
void user_run(const char *n); void user_list(void); proc_t *user_proc_create(const char *n, void (*e)(void));

/* ===== torFS ===== */
void tfs_init(void); int tfs_create(const char *n); int tfs_write(const char *n, const void *d, uint32 s, uint32 o);
int tfs_read(const char *n, void *b, uint32 s, uint32 o); int tfs_delete(const char *n);
int tfs_size(const char *n); void tfs_ls(void); void tfs_stat(void); void tfs_create_sample(void);

/* ===== RTC ===== */
void rtc_init(void); uint64 rtc_get_time(void); void rtc_time_string(char *b, int m);
void rtc_print_time(void); uint64 rtc_get_ticks(void); void rtc_udelay(uint32 u); void rtc_mdelay(uint32 m);

/* ===== Shell ===== */
void shell_run(void);

/* ===== Input Subsystem ===== */
#include "input.h"

/* ===== VirtIO Input ===== */
#include "virtio.h"
void virtio_input_init(void); void virtio_input_poll(void); int virtio_input_has_event(void);
virtio_input_event_t virtio_input_get_event(void); void virtio_input_shutdown(void);

/* ===== USB xHCI ===== */
#include "usb.h"
void usb_hotplug_init(void); void usb_hotplug_poll(void);
void usb_hid_init(void); void usb_hid_poll(void);

/* ===== Panic / Assert ===== */
#define panic(m) do { printk_color(TERM_RED, "\n[PANIC] %s at %s:%d\n", m, __FILE__, __LINE__); while(1) __asm__("wfe"); } while(0)
#define assert(c) do { if (!(c)) panic("Assertion failed: " #c); } while(0)

/* ===== Inline helpers ===== */
static inline uint64 r_mpidr(void) { uint64 x; __asm__("mrs %0, MPIDR_EL1" : "=r"(x)); return x; }
static inline uint64 r_currentel(void) { uint64 x; __asm__("mrs %0, CurrentEL" : "=r"(x)); return x; }
static inline uint64 r_cntvct(void) { uint64 x; __asm__("mrs %0, CNTVCT_EL0" : "=r"(x)); return x; }
static inline void wfi(void) { __asm__("wfi"); }
static inline void wfe(void) { __asm__("wfe"); }
static inline void isb(void) { __asm__("isb"); }
static inline void dsb(void) { __asm__("dsb sy"); }

/* ===== Missing keycodes (referenced in virtio_input.c) ===== */
#ifndef KEY_SYSRQ
#define KEY_SYSRQ       99
#endif
#ifndef KEY_SCROLLLOCK
#define KEY_SCROLLLOCK  70
#endif
#ifndef KEY_PAUSE
#define KEY_PAUSE       119
#endif
#ifndef KEY_NUMLOCK
#define KEY_NUMLOCK     69
#endif
#ifndef KEY_KPSLASH
#define KEY_KPSLASH     98
#endif
#ifndef KEY_KPPLUS
#define KEY_KPPLUS      78
#endif
#ifndef KEY_KPENTER
#define KEY_KPENTER     96
#endif
#ifndef KEY_KP1
#define KEY_KP1         79
#endif
#ifndef KEY_KP2
#define KEY_KP2         80
#endif
#ifndef KEY_KP3
#define KEY_KP3         81
#endif
#ifndef KEY_KP4
#define KEY_KP4         75
#endif
#ifndef KEY_KP5
#define KEY_KP5         76
#endif
#ifndef KEY_KP6
#define KEY_KP6         77
#endif
#ifndef KEY_KP7
#define KEY_KP7         71
#endif
#ifndef KEY_KP8
#define KEY_KP8         72
#endif
#ifndef KEY_KP9
#define KEY_KP9         73
#endif
#ifndef KEY_KP0
#define KEY_KP0         82
#endif
#ifndef KEY_KPDOT
#define KEY_KPDOT       83
#endif
#ifndef KEY_102ND
#define KEY_102ND       86
#endif
#ifndef KEY_KPMINUS
#define KEY_KPMINUS     74
#endif

#endif

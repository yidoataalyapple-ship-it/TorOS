/*
 * torOS Kernel Header v0.2
 */

#ifndef _TOROS_H
#define _TOROS_H

/* ---- Basic Types ---- */
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long       uint64;
typedef signed char         int8;
typedef signed short        int16;
typedef signed int          int32;
typedef signed long         int64;
typedef unsigned long       usize;
typedef unsigned long       uintptr;
typedef unsigned int        uint;
typedef unsigned int        bool;

#define true  1
#define false 0
#define NULL  ((void*)0)

/* ---- Memory ---- */
#define PAGE_SIZE       4096
#define KERNEL_BASE     0x40000000UL
#define RAM_SIZE        (2048UL * 1024 * 1024)

/* ---- Colors ---- */
#define TERM_BLACK      0
#define TERM_RED        1
#define TERM_GREEN      2
#define TERM_YELLOW     3
#define TERM_BLUE       4
#define TERM_MAGENTA    5
#define TERM_CYAN       6
#define TERM_WHITE      7

/* ---- Linker symbols ---- */
extern char _text_start[], _text_end[];
extern char _rodata_start[], _rodata_end[];
extern char _data_start[], _data_end[];
extern char _bss_start[], _bss_end[];
extern char _kernel_end[];
extern char _stack_top[];
extern char _dtb_ptr[];

/* ---- UART ---- */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(char *c);

/* ---- Printk ---- */
void printk(const char *fmt, ...);
void printk_color(int color, const char *fmt, ...);
void print_logo(void);

/* ---- Memory Management ---- */
void mm_init(void);
void *page_alloc(void);
void page_free(void *page);
usize get_free_pages(void);
void *kmalloc(usize size);
void kfree(void *ptr);

/* ---- String ---- */
void *memcpy(void *dst, const void *src, usize n);
void *memset(void *dst, int c, usize n);
int strcmp(const char *s1, const char *s2);
usize strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, usize n);
int strncmp(const char *s1, const char *s2, usize n);
char *itoa(int64 val, char *buf, int base);
char *utoa(uint64 val, char *buf, int base);

/* ---- Virtual Memory ---- */
void vm_init(void);
int vm_user_map(uint64 user_va, uint64 pa, uint64 flags);
void vm_user_unmap(uint64 user_va);
uint64 vm_va2pa(uint64 va);
void vm_flush_tlb(void);

/* ---- GICv3 ---- */
void gic_init(void);
void gic_enable_irq(uint irq);
void gic_disable_irq(uint irq);
void gic_set_priority(uint irq, uint8 prio);
void gic_set_target(uint irq, uint cpu);
uint gic_get_irq(void);
void gic_eoi(uint irq);

/* ---- Trap/Interrupt ---- */
void trap_init(void);
void timer_init(void);
void timer_set_next(void);
void trap_handle(void *regs);
void irq_handle(void);
void fiq_handle(void);
void error_handle(void);
uint64 get_jiffies(void);

/* ---- Process ---- */
#define NPROC       64
#define NAMELEN     16

typedef enum {
    PROC_UNUSED, PROC_RUNNABLE, PROC_RUNNING, PROC_SLEEPING, PROC_ZOMBIE,
} proc_state_t;

typedef struct context {
    uint64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64 fp, lr, sp;
} context_t;

typedef struct proc {
    uint64 pid;
    proc_state_t state;
    char name[NAMELEN];
    context_t ctx;
    uint64 *stack;
    uint64 sleep_until;
    struct proc *next;
} proc_t;

void sched_init(void);
void yield(void);
void sleep(uint64 ms);
void proc_table_dump(void);
proc_t *proc_create(const char *name, void (*entry)(void));
void schedule(void);

/* ---- Context Switch ---- */
void context_switch(context_t *old_ctx, context_t *new_ctx);
void context_first_switch(context_t *new_ctx);
void kernel_to_user(uint64 user_stack, uint64 user_entry, uint64 user_arg);

/* ---- Syscalls ---- */
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_EXIT    3
#define SYS_FORK    4
#define SYS_SLEEP   5
#define SYS_GETPID  6
#define SYS_EXEC    7
void syscall_handler(uint64 num, uint64 a0, uint64 a1, uint64 a2);

/* ---- Framebuffer ---- */
#define FB_WIDTH    1024
#define FB_HEIGHT   768

void fb_init(void);
void fb_clear(uint32 color);
void fb_putpixel(int x, int y, uint32 color);
void fb_draw_char(int x, int y, char c, uint32 color);
void fb_set_color(uint32 color);
void fb_draw_string(int x, int y, const char *s);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32 color);
void fb_draw_rect(int x, int y, int w, int h, uint32 color);
void fb_draw_border(int x, int y, int w, int h, uint32 color);
int fb_is_initialized(void);

/* ---- User Mode ---- */
void user_run(const char *name);
void user_list(void);
proc_t *user_proc_create(const char *name, void (*entry)(void));

/* ---- Shell ---- */
void shell_run(void);

/* ---- Panic ---- */
#define panic(msg) do { \
    printk_color(TERM_RED, "\n[PANIC] %s at %s:%d\n", msg, __FILE__, __LINE__); \
    while(1) { __asm__ volatile("wfe"); } \
} while(0)

#define assert(cond) do { if (!(cond)) panic("Assertion failed: " #cond); } while(0)

/* ---- Inline helpers ---- */
static inline uint64 r_mpidr(void) { uint64 x; __asm__("mrs %0, MPIDR_EL1" : "=r"(x)); return x; }
static inline uint64 r_currentel(void) { uint64 x; __asm__("mrs %0, CurrentEL" : "=r"(x)); return x; }
static inline uint64 r_cntvct(void) { uint64 x; __asm__("mrs %0, CNTVCT_EL0" : "=r"(x)); return x; }
static inline void wfi(void) { __asm__("wfi"); }
static inline void wfe(void) { __asm__("wfe"); }
static inline void isb(void) { __asm__("isb"); }
static inline void dsb(void) { __asm__("dsb sy"); }

#endif

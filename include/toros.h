/*
 * torOS Kernel Header
 * Main include file for all kernel modules
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
#define PAGE_SHIFT      12
#define KERNEL_BASE     0x40000000UL
#define RAM_SIZE        (2048UL * 1024 * 1024)  /* 2GB */

/* ---- Colors for terminal ---- */
#define TERM_BLACK      0
#define TERM_RED        1
#define TERM_GREEN      2
#define TERM_YELLOW     3
#define TERM_BLUE       4
#define TERM_MAGENTA    5
#define TERM_CYAN       6
#define TERM_WHITE      7
#define TERM_BRIGHT     8

/* ---- External symbols (from linker) ---- */
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

/* ---- Trap/Interrupt ---- */
void trap_init(void);
void timer_init(void);
void timer_set_next(void);

/* ---- Process ---- */
#define NPROC       64
#define NAMELEN     16
#define STACK_PAGES 2

typedef enum {
    PROC_UNUSED,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct context {
    uint64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64 fp;      // x29
    uint64 lr;      // x30
    uint64 sp;
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

/* ---- Scheduler ---- */
void scheduler(void) __attribute__((noreturn));
void schedule(void);

/* ---- Exception ---- */
void exception_handler(void);
void syscall_handler(uint64 num, uint64 a0, uint64 a1, uint64 a2);

/* ---- System Calls ---- */
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_EXIT    3
#define SYS_FORK    4
#define SYS_SLEEP   5
#define SYS_GETPID  6
#define SYS_EXEC    7

/* ---- Shell ---- */
void shell_run(void);

/* ---- Panic ---- */
#define panic(msg) do { \
    printk_color(TERM_RED, "\n[PANIC] %s at %s:%d\n", msg, __FILE__, __LINE__); \
    while(1) { __asm__ volatile("wfe"); } \
} while(0)

#define assert(cond) do { \
    if (!(cond)) panic("Assertion failed: " #cond); \
} while(0)

/* ---- Inline helpers ---- */
static inline uint64 r_mpidr(void)
{
    uint64 x;
    __asm__ volatile("mrs %0, MPIDR_EL1" : "=r"(x));
    return x;
}

static inline uint64 r_currentel(void)
{
    uint64 x;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(x));
    return x;
}

static inline uint64 r_cntvct(void)
{
    uint64 x;
    __asm__ volatile("mrs %0, CNTVCT_EL0" : "=r"(x));
    return x;
}

static inline void wfi(void)
{
    __asm__ volatile("wfi");
}

static inline void wfe(void)
{
    __asm__ volatile("wfe");
}

static inline void isb(void)
{
    __asm__ volatile("isb");
}

static inline void dsb(void)
{
    __asm__ volatile("dsb sy");
}

#endif /* _TOROS_H */

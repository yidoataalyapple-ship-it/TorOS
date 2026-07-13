/*
 * sched.h — Basit round-robin önleyici (preemptive) kernel scheduler
 * Trap-frame tabanlı context switch (timer IRQ üzerinden)
 * (Planda "MEVCUT" sayılan Scheduler/SMP bileşeninin ilk sürümü)
 */
#ifndef TOROS_SCHED_H
#define TOROS_SCHED_H

#include <toros/types.h>
#include <toros/irq.h>

#define SCHED_MAX_TASKS 16
#define TASK_STACK_SIZE (16 * KB)

enum task_state {
    TASK_FREE = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD,
};

struct task {
    struct trap_frame *tf;     /* stack üzerindeki kayıtlı trap frame */
    u64  stack_base;
    u64  stack_top;
    pid_t pid;
    char name[32];
    enum task_state state;
    u64  wake_tick;            /* sleep bitiş tick'i */
    u64  cpu_ticks;            /* toplam çalışma tick'i (yaklaşık) */
    int  is_idle;
};

typedef void (*task_fn_t)(void *arg);

void  sched_init(void);
void  sched_start(void) __attribute__((noreturn));   /* scheduler'ı devreye al */
pid_t task_create(const char *name, task_fn_t fn, void *arg);
void  task_exit(void) __attribute__((noreturn));
void  task_yield(void);
void  task_sleep_ms(u64 ms);
struct task *task_current(void);
struct task *task_table(void);
u32   task_count(void);

/* Timer IRQ'sundan çağrılır: gerekirse task değiştirir, yeni tf döner */
struct trap_frame *sched_tick(struct trap_frame *tf);

#endif

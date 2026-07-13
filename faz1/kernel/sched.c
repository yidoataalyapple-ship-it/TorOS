/*
 * sched.c — Round-robin önleyici kernel scheduler
 *
 * Context switch: timer IRQ -> trap frame kaydet/seç/geri yükle.
 * Yeni task'lar sahte bir trap frame ile başlatılır (EL1h, IRQ açık).
 */
#include <toros/sched.h>
#include <toros/timer.h>
#include <toros/mm.h>
#include <toros/printf.h>
#include <toros/string.h>

static struct task tasks[SCHED_MAX_TASKS];
static u32 n_tasks;
static u32 cur_idx;
static int  sched_active;
static pid_t next_pid = 1;

#define SPSR_KERNEL_THREAD 0x345UL   /* EL1h, FIQ/SError/Debug maskeli, IRQ açık */

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    /* Task 0: boot/idle bağlamı */
    tasks[0].pid = next_pid++;
    strncpy(tasks[0].name, "idle", sizeof(tasks[0].name) - 1);
    tasks[0].state = TASK_RUNNING;
    tasks[0].is_idle = 1;
    n_tasks = 1;
    cur_idx = 0;
    sched_active = 0;
}

static void task_trampoline(task_fn_t fn, void *arg)
{
    fn(arg);
    task_exit();
}

pid_t task_create(const char *name, task_fn_t fn, void *arg)
{
    irq_disable();
    if (n_tasks >= SCHED_MAX_TASKS) {
        irq_enable();
        kerr("task_create: tablo dolu\n");
        return -1;
    }

    struct task *t = &tasks[n_tasks];
    memset(t, 0, sizeof(*t));

    u8 *stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        irq_enable();
        return -1;
    }
    t->stack_base = (u64)stack;
    t->stack_top = ALIGN_DOWN((u64)stack + TASK_STACK_SIZE, 16);

    /* Sahte trap frame */
    u64 sp = t->stack_top - TRAP_FRAME_SIZE;
    struct trap_frame *tf = (struct trap_frame *)sp;
    memset(tf, 0, sizeof(*tf));
    tf->x[0] = (u64)fn;             /* arg0 -> trampoline */
    tf->x[1] = (u64)arg;
    tf->elr = (u64)task_trampoline;
    tf->x[30] = (u64)task_exit;     /* fn dönerse */
    tf->spsr = SPSR_KERNEL_THREAD;

    t->tf = tf;
    t->pid = next_pid++;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->state = TASK_READY;

    pid_t pid = t->pid;
    n_tasks++;
    irq_enable();
    return pid;
}

struct task *task_current(void) { return &tasks[cur_idx]; }
struct task *task_table(void)   { return tasks; }
u32 task_count(void)            { return n_tasks; }

void task_exit(void)
{
    irq_disable();
    tasks[cur_idx].state = TASK_DEAD;
    kinfo("task '%s' (pid %d) sonlandı\n", tasks[cur_idx].name, tasks[cur_idx].pid);
    irq_enable();
    /* Bir daha seçilmeyecek; scheduler ilk tick'te başkasına geçer */
    for (;;)
        wfe();
}

void task_yield(void)
{
    wfe();
}

void task_sleep_ms(u64 ms)
{
    u64 deadline = timer_ticks() + (ms * TIMER_HZ) / 1000 + 1;
    irq_disable();
    tasks[cur_idx].wake_tick = deadline;
    tasks[cur_idx].state = TASK_SLEEPING;
    irq_enable();
    while (tasks[cur_idx].state == TASK_SLEEPING)
        wfe();
}

struct trap_frame *sched_tick(struct trap_frame *tf)
{
    if (!sched_active || n_tasks <= 1)
        return tf;

    /* Mevcut task'ın frame'ini kaydet */
    tasks[cur_idx].tf = tf;
    tasks[cur_idx].cpu_ticks++;

    /* Uyanma zamanı gelenleri hazırla */
    u64 now = timer_ticks();
    for (u32 i = 0; i < n_tasks; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick)
            tasks[i].state = TASK_READY;
    }

    /* Round-robin: sonraki READY task */
    u32 next = cur_idx;
    for (u32 i = 1; i <= n_tasks; i++) {
        u32 j = (cur_idx + i) % n_tasks;
        if (tasks[j].state == TASK_READY) {
            next = j;
            break;
        }
    }

    if (next == cur_idx)
        return tf;

    if (tasks[cur_idx].state == TASK_RUNNING)
        tasks[cur_idx].state = TASK_READY;
    cur_idx = next;
    tasks[cur_idx].state = TASK_RUNNING;
    return tasks[cur_idx].tf;
}

void sched_start(void)
{
    sched_active = 1;
    kok("Scheduler aktif (%u task)\n", n_tasks);
    /* idle döngüsü: IRQ ile uyan, task'lar çalışsın */
    for (;;)
        wfi();
}

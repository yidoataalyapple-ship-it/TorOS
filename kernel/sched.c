/*
 * torOS Process Scheduler
 * Round-robin preemptive scheduling
 */

#include "../include/toros.h"

proc_t proc_table[NPROC];
int proc_count = 0;
static proc_t *current = NULL;
static proc_t *ready_queue = NULL;
static uint64 next_pid = 1;

extern uint64 get_jiffies(void);

void sched_init(void)
{
    memset(proc_table, 0, sizeof(proc_table));
    for (int i = 0; i < NPROC; i++)
        proc_table[i].state = PROC_UNUSED;
    current = NULL;
    ready_queue = NULL;
}

static void enqueue(proc_t *p)
{
    p->next = NULL;
    if (!ready_queue) {
        ready_queue = p;
        return;
    }
    proc_t *q = ready_queue;
    while (q->next)
        q = q->next;
    q->next = p;
}

static proc_t *dequeue(void)
{
    if (!ready_queue)
        return NULL;
    proc_t *p = ready_queue;
    ready_queue = ready_queue->next;
    p->next = NULL;
    return p;
}

proc_t *proc_create(const char *name, void (*entry)(void))
{
    for (int i = 0; i < NPROC; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            proc_t *p = &proc_table[i];
            proc_count++;
            p->pid = next_pid++;
            p->state = PROC_RUNNABLE;
            strncpy(p->name, name, NAMELEN - 1);
            p->name[NAMELEN - 1] = '\0';

            /* Allocate stack */
            p->stack = page_alloc();
            if (!p->stack) {
                p->state = PROC_UNUSED;
                return NULL;
            }

            /* Setup context - stack grows down */
            uint64 *sp = (uint64 *)((uint8 *)p->stack + PAGE_SIZE);
            sp[0] = (uint64)entry;     /* x30 (lr) */
            sp[-1] = 0;                 /* x29 (fp) */
            p->ctx.sp = (uint64)sp;
            p->ctx.lr = (uint64)entry;
            p->ctx.x19 = 0;
            p->ctx.x20 = 0;
            p->ctx.x21 = 0;
            p->ctx.x22 = 0;
            p->ctx.x23 = 0;
            p->ctx.x24 = 0;
            p->ctx.x25 = 0;
            p->ctx.x26 = 0;
            p->ctx.x27 = 0;
            p->ctx.x28 = 0;
            p->ctx.fp = 0;

            enqueue(p);
            return p;
        }
    }
    return NULL;
}

void yield(void)
{
    schedule();
}

void sleep(uint64 ms)
{
    if (!current)
        return;
    current->sleep_until = get_jiffies() + ms;
    current->state = PROC_SLEEPING;
    schedule();
}

void schedule(void)
{
    /* Wake up sleeping processes */
    uint64 now = get_jiffies();
    for (int i = 0; i < NPROC; i++) {
        if (proc_table[i].state == PROC_SLEEPING &&
            proc_table[i].sleep_until <= now) {
            proc_table[i].state = PROC_RUNNABLE;
            enqueue(&proc_table[i]);
        }
    }

    /* If current still running, put back */
    if (current && current->state == PROC_RUNNING) {
        current->state = PROC_RUNNABLE;
        enqueue(current);
    }

    /* Pick next */
    proc_t *next = dequeue();
    if (!next) {
        /* No runnable - keep current or idle */
        if (current && current->state == PROC_RUNNABLE) {
            current->state = PROC_RUNNING;
            return;
        }
        /* Idle loop */
        while (!(next = dequeue()))
            wfi();
    }

    next->state = PROC_RUNNING;
    current = next;

    /* Context switch would happen here */
    /* For now, simple cooperative scheduling via timer */
}

void proc_table_dump(void)
{
    printk_color(TERM_CYAN, "  PID  STATE       NAME\n");
    printk_color(TERM_CYAN, "  ---  ----------  --------\n");
    for (int i = 0; i < NPROC; i++) {
        if (proc_table[i].state == PROC_UNUSED)
            continue;
        const char *s;
        switch (proc_table[i].state) {
        case PROC_RUNNABLE: s = "RUNNABLE  "; break;
        case PROC_RUNNING:  s = "RUNNING   "; break;
        case PROC_SLEEPING: s = "SLEEPING  "; break;
        case PROC_ZOMBIE:   s = "ZOMBIE    "; break;
        default:            s = "UNKNOWN   "; break;
        }
        printk("  %d   %s  %s\n",
               proc_table[i].pid, s, proc_table[i].name);
    }
}

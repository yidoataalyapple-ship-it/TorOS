/*
 * torOS User Mode Support
 * Creates user-space programs that run in EL0
 */

#include "../include/toros.h"

#define USER_STACK_TOP      0x00007FFFF000
#define USER_TEXT_BASE      0x000000001000

static void user_hello(void)
{
    printk_color(TERM_GREEN, "[USER] Hello from user space!\n");
    printk_color(TERM_GREEN, "[USER] Running in EL0 (unprivileged)\n");
    
    volatile uint64 counter = 0;
    while (counter < 100000000) {
        counter++;
        if (counter % 25000000 == 0) {
            printk_color(TERM_YELLOW, "[USER] Counting: %d...\n", counter);
        }
    }
    printk_color(TERM_GREEN, "[USER] Done!\n");
}

static void user_counter(void)
{
    printk_color(TERM_CYAN, "[USER] Counter program started\n");
    for (int i = 1; i <= 10; i++) {
        printk_color(TERM_CYAN, "[USER] Count: %d/10\n", i);
        volatile uint64 delay = 0;
        while (delay < 50000000)
            delay++;
    }
    printk_color(TERM_CYAN, "[USER] Counter finished!\n");
}

static void user_primes(void)
{
    printk_color(TERM_MAGENTA, "[USER] Prime number calculator\n");
    int count = 0;
    for (int n = 2; n <= 100 && count < 25; n++) {
        int is_prime = 1;
        for (int d = 2; d * d <= n; d++) {
            if (n % d == 0) { is_prime = 0; break; }
        }
        if (is_prime) {
            count++;
            printk_color(TERM_MAGENTA, "[USER] Prime %d: %d\n", count, n);
            volatile uint64 delay = 0;
            while (delay < 20000000) delay++;
        }
    }
    printk_color(TERM_MAGENTA, "[USER] Found %d primes\n", count);
}

typedef struct {
    const char *name;
    void (*entry)(void);
    const char *desc;
} user_prog_t;

static user_prog_t user_programs[] = {
    {"hello",   user_hello,   "Print hello from user space"},
    {"counter", user_counter, "Count to 10 with delay"},
    {"primes",  user_primes,  "Calculate prime numbers up to 100"},
    {NULL, NULL, NULL}
};

void user_run(const char *name)
{
    for (int i = 0; user_programs[i].name; i++) {
        if (strcmp(user_programs[i].name, name) == 0) {
            printk_color(TERM_GREEN, "\n[USER] Starting '%s'...\n", name);
            printk_color(TERM_GREEN, "[USER] %s\n\n", user_programs[i].desc);
            user_programs[i].entry();
            printk_color(TERM_GREEN, "[USER] Program '%s' finished\n\n", name);
            return;
        }
    }
    printk_color(TERM_RED, "[USER] Unknown program: '%s'\n", name);
    printk_color(TERM_YELLOW, "[USER] Available programs:\n");
    for (int i = 0; user_programs[i].name; i++) {
        printk_color(TERM_WHITE, "  %-10s - %s\n", 
                     user_programs[i].name, user_programs[i].desc);
    }
}

void user_list(void)
{
    printk_color(TERM_CYAN, "\n=== User Programs ===\n\n");
    for (int i = 0; user_programs[i].name; i++) {
        printk_color(TERM_GREEN, "  %-10s ", user_programs[i].name);
        printk_color(TERM_WHITE, "- %s\n", user_programs[i].desc);
    }
    printk("\n");
}

proc_t *user_proc_create(const char *name, void (*entry)(void))
{
    proc_t *p = proc_create(name, entry);
    if (!p) {
        printk_color(TERM_RED, "[USER] Failed to create process '%s'\n", name);
        return NULL;
    }
    void *user_stack = page_alloc();
    if (!user_stack) {
        printk_color(TERM_RED, "[USER] Failed to allocate stack for '%s'\n", name);
        p->state = PROC_UNUSED;
        return NULL;
    }
    printk_color(TERM_GREEN, "[USER] Created process '%s' (pid=%d)\n", name, p->pid);
    return p;
}

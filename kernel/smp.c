/*
 * torOS SMP - Symmetric Multi-Processing
 * 4x Cortex-A72 multi-core support
 */

#include "../include/toros.h"

#define MAX_CPUS        4

typedef struct {
    uint64 cpu_id;
    proc_t *current;
    proc_t *idle_task;
    uint64 irq_count;
    uint64 ctx_switches;
    int online;
} cpu_data_t;

static cpu_data_t cpu_data[MAX_CPUS];
static int num_online_cpus = 0;
static spinlock_t smp_lock;

static int psci_cpu_on(uint64 target_cpu, uint64 entry, uint64 context)
{
    register uint64 x0 __asm__("x0") = 0xC4000003;
    register uint64 x1 __asm__("x1") = target_cpu;
    register uint64 x2 __asm__("x2") = entry;
    register uint64 x3 __asm__("x3") = context;
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3));
    return x0;
}

void smp_secondary_start(void)
{
    uint64 cpu = r_mpidr() & 0xFF;
    printk_color(TERM_GREEN, "[SMP] CPU %d online\n", cpu);
    
    uint64 stack = (uint64)page_alloc();
    if (!stack) { printk_color(TERM_RED, "[SMP] CPU %d no stack\n", cpu); while (1) wfi(); }
    __asm__ volatile("mov sp, %0" :: "r"(stack + PAGE_SIZE));
    
    gic_init();
    trap_init();
    timer_init();
    
    cpu_data[cpu].cpu_id = cpu;
    cpu_data[cpu].online = 1;
    cpu_data[cpu].irq_count = 0;
    cpu_data[cpu].ctx_switches = 0;
    num_online_cpus++;
    
    while (1) { wfi(); schedule(); }
}

void smp_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Initializing SMP...\n");
    spin_init(&smp_lock);
    
    cpu_data[0].cpu_id = 0;
    cpu_data[0].online = 1;
    cpu_data[0].current = NULL;
    num_online_cpus = 1;
    
    for (int i = 1; i < MAX_CPUS; i++) {
        printk_color(TERM_YELLOW, "[SMP] Booting CPU %d...\n", i);
        int ret = psci_cpu_on(i, (uint64)smp_secondary_start, 0);
        if (ret == 0) printk_color(TERM_GREEN, "[SMP] CPU %d booted\n", i);
        else printk_color(TERM_RED, "[SMP] CPU %d failed: %d\n", i, ret);
    }
    
    volatile uint64 delay = 0;
    while (delay < 100000000) delay++;
    
    printk_color(TERM_GREEN, "[BOOT] SMP: %d/%d CPUs online\n", num_online_cpus, MAX_CPUS);
}

void smp_send_ipi(uint target_cpu, uint ipi_type)
{
    (void)ipi_type;
    if (target_cpu >= MAX_CPUS || !cpu_data[target_cpu].online) return;
    volatile uint32 *gicd_sgi = (volatile uint32 *)(0x08000000 + 0xF00);
    *gicd_sgi = (1 << 24) | (0 << 0) | ((target_cpu & 0xF) << 16);
}

void smp_broadcast_ipi(uint ipi_type)
{
    for (int i = 0; i < MAX_CPUS; i++)
        if (i != (int)(r_mpidr() & 0xFF) && cpu_data[i].online)
            smp_send_ipi(i, ipi_type);
}

int smp_cpu_count(void) { return num_online_cpus; }

void smp_dump(void)
{
    printk_color(TERM_CYAN, "\n  SMP Status:\n");
    printk("  CPUs: %d/%d\n", num_online_cpus, MAX_CPUS);
    for (int i = 0; i < MAX_CPUS; i++) {
        uint32 c = cpu_data[i].online ? TERM_GREEN : TERM_RED;
        printk_color(c, "  CPU %d: %s", i, cpu_data[i].online ? "ONLINE" : "OFFLINE");
        if (cpu_data[i].online)
            printk_color(TERM_WHITE, " | IRQ:%d CTX:%d", cpu_data[i].irq_count, cpu_data[i].ctx_switches);
        printk("\n");
    }
    printk("\n");
}

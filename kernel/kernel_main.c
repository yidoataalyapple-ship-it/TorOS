/*
 * torOS Kernel Main v0.3
 */

#include "../include/toros.h"

void kernel_main(void)
{
    uart_init();
    print_logo();

    printk_color(TERM_YELLOW, "[BOOT] Memory manager...\n");
    mm_init();

    printk_color(TERM_YELLOW, "[BOOT] Virtual memory...\n");
    vm_init();

    printk_color(TERM_YELLOW, "[BOOT] GICv3...\n");
    gic_init();

    printk_color(TERM_YELLOW, "[BOOT] Trap handlers...\n");
    trap_init();

    printk_color(TERM_YELLOW, "[BOOT] Timer...\n");
    timer_init();

    printk_color(TERM_YELLOW, "[BOOT] RTC...\n");
    rtc_init();

    printk_color(TERM_YELLOW, "[BOOT] Scheduler...\n");
    sched_init();

    printk_color(TERM_YELLOW, "[BOOT] Framebuffer...\n");
    fb_init();

    printk_color(TERM_YELLOW, "[BOOT] torFS...\n");
    tfs_init();
    tfs_create_sample();

    printk_color(TERM_YELLOW, "[BOOT] SMP...\n");
    smp_init();

    printk("\n");
    printk_color(TERM_GREEN, "========================================\n");
    printk_color(TERM_GREEN, "  torOS v0.3.0 boot complete!\n");
    printk_color(TERM_GREEN, "========================================\n");
    printk("\n");
    printk_color(TERM_CYAN, "[SYS] Arch: ARM64, CPU: Cortex-A72 x%d\n", smp_cpu_count());
    printk_color(TERM_CYAN, "[SYS] RAM: 2GB, EL: %d\n", (r_currentel() >> 2) & 3);
    rtc_print_time();
    printk("\n");
    printk_color(TERM_GREEN, "Features: MMU GICv3 SMP torFS FB SCHED VM SPINLOCK\n\n");

    proc_table_dump();
    printk("\n");
    tfs_ls();

    printk_color(TERM_GREEN, "[BOOT] Starting shell...\n\n");
    shell_run();

    panic("kernel_main returned!");
}

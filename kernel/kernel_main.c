/*
 * torOS Kernel Main
 * Entry point from assembly
 */

#include "../include/toros.h"

void kernel_main(void)
{
    /* 1. Initialize UART for serial output */
    uart_init();

    /* Print boot banner */
    print_logo();

    /* 2. Initialize memory management */
    printk_color(TERM_YELLOW, "[BOOT] Initializing memory management...\n");
    mm_init();
    printk_color(TERM_GREEN, "[BOOT] Memory: %d pages (%d MB) available\n",
                 get_free_pages(), (get_free_pages() * PAGE_SIZE) / (1024 * 1024));

    /* 3. Initialize interrupts/traps */
    printk_color(TERM_YELLOW, "[BOOT] Initializing trap handlers...\n");
    trap_init();
    printk_color(TERM_GREEN, "[BOOT] Trap handlers installed\n");

    /* 4. Initialize timer */
    printk_color(TERM_YELLOW, "[BOOT] Initializing timer...\n");
    timer_init();
    printk_color(TERM_GREEN, "[BOOT] Timer running\n");

    /* 5. Initialize scheduler */
    printk_color(TERM_YELLOW, "[BOOT] Initializing scheduler...\n");
    sched_init();
    printk_color(TERM_GREEN, "[BOOT] Scheduler ready\n");

    /* 6. Print system info */
    printk("\n");
    printk_color(TERM_CYAN, "[INFO] torOS booted successfully on ARM64\n");
    printk_color(TERM_CYAN, "[INFO] CPU ID: %x\n", r_mpidr());
    printk_color(TERM_CYAN, "[INFO] Current EL: %d\n", (r_currentel() >> 2) & 3);
    printk("\n");

    /* 7. Dump process table */
    proc_table_dump();
    printk("\n");

    /* 8. Start shell */
    printk_color(TERM_GREEN, "[BOOT] Starting torOS Shell...\n\n");
    shell_run();

    /* Should never reach here */
    panic("kernel_main returned!");
}

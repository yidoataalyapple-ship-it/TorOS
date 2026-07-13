/*
 * torOS Kernel Main v0.4
 * Full desktop OS initialization
 */

#include "../include/toros.h"
#include "../include/gpu.h"
#include "../include/window.h"

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

    printk_color(TERM_YELLOW, "[BOOT] Input Event Subsystem...\n");
    input_subsystem_init();

    printk_color(TERM_YELLOW, "[BOOT] VirtIO Input (Keyboard/Mouse)...\n");
    virtio_input_init();

    printk_color(TERM_YELLOW, "[BOOT] USB xHCI...\n");
    xhci_init(XHCI_MMIO_BASE);

    printk_color(TERM_YELLOW, "[BOOT] USB HID...\n");
    usb_hid_init();

    printk_color(TERM_YELLOW, "[BOOT] USB Hot-plug...\n");
    usb_hotplug_init();

    printk_color(TERM_YELLOW, "[BOOT] Scheduler...\n");
    sched_init();

    printk_color(TERM_YELLOW, "[BOOT] Framebuffer...\n");
    fb_init();

    printk_color(TERM_YELLOW, "[BOOT] GPU Subsystem (VirtIO-GPU)...\n");
    gpu_subsystem_init();

    printk_color(TERM_YELLOW, "[BOOT] Window Manager...\n");
    wm_init(FB_WIDTH, FB_HEIGHT);

    printk_color(TERM_YELLOW, "[BOOT] Compositor...\n");
    compositor_init(FB_WIDTH, FB_HEIGHT);

    printk_color(TERM_YELLOW, "[BOOT] Desktop Shell...\n");
    desktop_shell_init(FB_WIDTH, FB_HEIGHT);

    printk_color(TERM_YELLOW, "[BOOT] Virtual Desktops (4)...\n");
    vd_init(4);

    printk_color(TERM_YELLOW, "[BOOT] torFS...\n");
    tfs_init();
    tfs_create_sample();

    printk_color(TERM_YELLOW, "[BOOT] SMP...\n");
    smp_init();

    printk("\n");
    printk_color(TERM_GREEN, "========================================\n");
    printk_color(TERM_GREEN, "  torOS v0.4.0 boot complete!\n");
    printk_color(TERM_GREEN, "========================================\n");
    printk("\n");
    printk_color(TERM_CYAN, "[SYS] Arch: ARM64, CPU: Cortex-A72 x%d\n", smp_cpu_count());
    printk_color(TERM_CYAN, "[SYS] RAM: 2GB, EL: %d\n", (r_currentel() >> 2) & 3);
    rtc_print_time();
    printk("\n");
    printk_color(TERM_GREEN, "Features: MMU GICv3 SMP torFS FB SCHED VM SPINLOCK\n");
    printk_color(TERM_GREEN, "          INPUT VIRTIO-INPUT USB-XHCI USB-HID HOTPLUG\n");
    printk_color(TERM_GREEN, "          VIRTIO-GPU WM COMPOSITOR DESKTOP VDESKTOP\n\n");

    proc_table_dump();
    printk("\n");
    tfs_ls();

    printk_color(TERM_GREEN, "[BOOT] Starting shell...\n\n");
    shell_run();

    panic("kernel_main returned!");
}

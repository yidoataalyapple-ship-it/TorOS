/*
 * main.c — TorOS kernel giriş noktası
 *
 * Boot sırası:
 *   UART -> MMU -> GICv3 -> Timer -> Heap -> torFS -> Input altyapısı
 *   -> PCI -> virtio-input (Faz 1.1/1.2) -> /dev/input/eventN (Faz 1.4)
 *   -> Scheduler -> Shell
 */
#include <toros/types.h>
#include <toros/uart.h>
#include <toros/printf.h>
#include <toros/mmu.h>
#include <toros/gic.h>
#include <toros/timer.h>
#include <toros/mm.h>
#include <toros/pci.h>
#include <toros/virtio_input.h>
#include <toros/input.h>
#include <toros/keyboard.h>
#include <toros/mouse.h>
#include <toros/torfs.h>
#include <toros/sched.h>
#include <toros/shell.h>
#include <toros/string.h>

#define TOROS_VERSION "0.1.0-faz1"

/* /dev/input/eventN aygıt okuma callback'i */
static int dev_input_read(void *arg, void *buf, size_t len)
{
    struct input_dev *dev = arg;
    if (len < sizeof(struct input_event))
        return -1;
    struct input_event ev;
    if (input_read_event(dev, &ev) != 0)
        return -1;   /* boş */
    memcpy(buf, &ev, sizeof(ev));
    return (int)sizeof(ev);
}

static void register_devfs(void)
{
    for (u32 i = 0; i < input_device_count(); i++) {
        struct input_dev *dev = input_get_device(i);
        char path[64];
        /* "/dev/input/eventN" yolu (flat isim alanı, / ayracı taklidi) */
        path[0] = 0;
        strcat(path, "/dev/input/event");
        char num[8];
        num[0] = '0' + (char)(i % 10);
        num[1] = 0;
        strcat(path, num);
        torfs_create_device(path, dev_input_read, dev);
        kok("devfs: %s -> %s\n", path, dev->name);
    }
}

/* Heartbeat demo task'ı: scheduler'ın çalıştığını gösterir */
static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;) {
        task_sleep_ms(10000);
        kprintf(KCLR_GRAY "[hb] uptime %lu sn, heap %lu KB, tick %lu\n" KCLR_RESET,
                timer_uptime_ms() / 1000, mm_heap_used() / KB, timer_ticks());
    }
}

void kernel_main(u64 dtb)
{
    (void)dtb;

    uart_init();
    kprintf("\n");
    kprintf(KCLR_CYAN "========================================\n" KCLR_RESET);
    kprintf(KCLR_CYAN "  TorOS %s — ARM64 boot\n" KCLR_RESET, TOROS_VERSION);
    kprintf(KCLR_CYAN "========================================\n" KCLR_RESET);

    kinfo("EL%lu seviyesinde çalışıyor\n", read_current_el());

    mm_init();
    mmu_init();
    gic_init();
    timer_init();

    /* Dosya sistemi + input altyapısı */
    torfs_init();
    input_init();
    keyboard_init();
    mouse_init();

    /* PCI + virtio-input (Faz 1.1 / 1.2) */
    pci_init();
    virtio_input_init();

    /* /dev/input/eventN sanal dosyaları (Faz 1.4) */
    register_devfs();

    /* Demo dosya */
    torfs_create("/etc/motd");
    const char *motd = "TorOS'a hos geldiniz! Faz 1: Input Devices tamam.\n";
    torfs_write("/etc/motd", motd, strlen(motd), 0);

    /* Scheduler + task'lar */
    sched_init();
    task_create("shell", shell_run, NULL);
    task_create("heartbeat", heartbeat_task, NULL);

    kok("Boot tamam — IRQ'lar açılıyor\n\n");

    irq_enable();
    sched_start();   /* dönmez */
}

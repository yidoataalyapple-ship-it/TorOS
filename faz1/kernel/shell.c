/*
 * shell.c — TorOS demo shell'i
 *
 * Girdi: UART (serial) + virtio klavye (Faz 1.1)
 * Komutlar: help, ls, cat, write, rm, events, mouse, kbdstat,
 *           mem, ps, uptime, pci, uname, clear
 */
#include <toros/shell.h>
#include <toros/printf.h>
#include <toros/uart.h>
#include <toros/keyboard.h>
#include <toros/mouse.h>
#include <toros/input.h>
#include <toros/torfs.h>
#include <toros/timer.h>
#include <toros/mm.h>
#include <toros/sched.h>
#include <toros/pci.h>
#include <toros/string.h>

#define LINE_MAX 256
static char line[LINE_MAX];
static u32 line_len;

static const char *banner =
    KCLR_CYAN
    "\n"
    "  ████████╗ ██████╗ ██████╗  ██████╗ ███████╗\n"
    "  ╚══██╔══╝██╔═══██╗██╔══██╗██╔═══██╗██╔════╝\n"
    "     ██║   ██║   ██║██████╔╝██║   ██║███████╗\n"
    "     ██║   ██║   ██║██╔══██╗██║   ██║╚════██║\n"
    "     ██║   ╚██████╔╝██║  ██║╚██████╔╝███████║\n"
    "     ╚═╝    ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝\n"
    KCLR_RESET
    "  ARM64 deneysel işletim sistemi — Faz 1 (Input)\n";

static void prompt(void)
{
    kprintf(KCLR_GREEN "toros" KCLR_RESET KCLR_GRAY ">" KCLR_RESET " ");
}

static void cmd_help(void)
{
    kprintf("Komutlar:\n");
    kprintf("  help              - bu yardım\n");
    kprintf("  ls [ön ek]        - torFS dosyalarını listele\n");
    kprintf("  cat <dosya>       - dosya içeriğini yazdır\n");
    kprintf("  write <dosya> <metin> - dosya oluştur/yaz\n");
    kprintf("  rm <dosya>        - dosya sil\n");
    kprintf("  events [n]        - n input event oku (varsayılan 5)\n");
    kprintf("  mouse             - fare durumu (x, y, butonlar, wheel)\n");
    kprintf("  kbdstat           - klavye istatistikleri + modifier'lar\n");
    kprintf("  mem               - heap kullanımı\n");
    kprintf("  ps                - task listesi\n");
    kprintf("  pci               - PCI aygıtları\n");
    kprintf("  uptime            - çalışma süresi\n");
    kprintf("  uname             - sistem bilgisi\n");
    kprintf("  clear             - ekranı temizle\n");
}

static void cmd_ls(const char *args)
{
    struct torfs_stat st[64];
    const char *prefix = NULL;
    if (args && *args)
        prefix = args;

    int n = torfs_list(prefix, st, 64);
    if (n == 0) {
        kprintf("(boş)\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        const char *t = st[i].type == TORFS_T_DEVICE ? "dev" : "file";
        kprintf("  %-4s %-40s %u bayt\n", t, st[i].path, st[i].size);
    }
    kprintf("%d dosya\n", n);
}

static void cmd_cat(const char *args)
{
    if (!args || !*args) {
        kprintf("kullanım: cat <dosya>\n");
        return;
    }
    char buf[512];
    struct torfs_stat st;
    if (torfs_stat(args, &st) != 0) {
        kprintf("dosya yok: %s\n", args);
        return;
    }
    if (st.type == TORFS_T_DEVICE) {
        kprintf("aygıt dosyası: 'events' komutu ile okuyun\n");
        return;
    }
    u32 off = 0;
    int r;
    while ((r = torfs_read(args, buf, sizeof(buf) - 1, off)) > 0) {
        buf[r] = 0;
        kprintf("%s", buf);
        off += r;
    }
    kprintf("\n");
}

static void cmd_write(const char *args)
{
    if (!args || !*args) {
        kprintf("kullanım: write <dosya> <metin>\n");
        return;
    }
    char path[96];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 95) {
        path[i] = args[i];
        i++;
    }
    path[i] = 0;
    const char *text = args[i] ? args + i + 1 : "";

    if (torfs_create(path) < 0) {
        /* zaten var olabilir, sorun değil */
    }
    int w = torfs_write(path, text, strlen(text), 0);
    if (w < 0)
        kprintf("yazılamadı: %s\n", path);
    else
        kprintf("%d bayt yazıldı: %s\n", w, path);
}

static void cmd_rm(const char *args)
{
    if (!args || !*args) {
        kprintf("kullanım: rm <dosya>\n");
        return;
    }
    if (torfs_delete(args) == 0)
        kprintf("silindi: %s\n", args);
    else
        kprintf("silinemedi: %s\n", args);
}

static void cmd_events(const char *args)
{
    int n = 5;
    if (args && *args)
        n = atoi(args);
    if (n <= 0 || n > 64)
        n = 5;

    kprintf("%d event bekleniyor (klavye/fare kullanın, 10sn timeout/olay)...\n", n);
    for (int i = 0; i < n; i++) {
        struct input_event ev;
        int dev = input_read_any(&ev, 10000);
        if (dev < 0) {
            kprintf("  timeout\n");
            return;
        }
        const char *type = ev.type == EV_KEY ? "KEY" :
                           ev.type == EV_REL ? "REL" :
                           ev.type == EV_ABS ? "ABS" :
                           ev.type == EV_SYN ? "SYN" : "?";
        kprintf("  [event%d] %5lu.%06lu  %-3s code=%3u value=%d\n",
                dev, ev.time_sec, ev.time_usec, type, ev.code, ev.value);
    }
}

static void cmd_mouse(void)
{
    s32 x, y;
    u8 btn;
    mouse_get_state(&x, &y, &btn);
    kprintf("Fare: x=%d y=%d butonlar=0x%x (L=%d R=%d M=%d) wheel=%d olay=%lu\n",
            x, y, btn,
            !!(btn & 1), !!(btn & 2), !!(btn & 4),
            mouse_wheel_delta(), mouse_event_count());
}

static void cmd_kbdstat(void)
{
    u8 m = keyboard_modifiers();
    kprintf("Klavye: %u olay işlendi, modifier=0x%x (shift=%d ctrl=%d alt=%d caps=%d)\n",
            keyboard_event_count(), m,
            !!(m & 1), !!(m & 2), !!(m & 4), !!(m & 8));
}

static void cmd_mem(void)
{
    kprintf("Heap: %lu KB / %lu MB kullanımda\n",
            mm_heap_used() / KB, mm_heap_total() / MB);
}

static void cmd_ps(void)
{
    struct task *t = task_table();
    u32 n = task_count();
    kprintf("  PID  DURUM      CPU(tick)  AD\n");
    for (u32 i = 0; i < n; i++) {
        if (t[i].state == TASK_FREE)
            continue;
        const char *st = t[i].state == TASK_RUNNING  ? "running" :
                         t[i].state == TASK_READY    ? "ready"   :
                         t[i].state == TASK_SLEEPING ? "sleep"   : "dead";
        kprintf("  %-4d %-10s %-10lu %s\n", t[i].pid, st, t[i].cpu_ticks, t[i].name);
    }
}

static void cmd_pci(void)
{
    kprintf("  BUS:DEV.FN  VENDOR:DEVICE  CLASS  IRQ\n");
    for (int i = 0; i < pci_device_count(); i++) {
        const struct pci_device *d = pci_get_device(i);
        kprintf("  %02x:%02x.%d     %04x:%04x       %02x:%02x  line %d (INTID %d)\n",
                d->bus, d->dev, d->fn, d->vendor, d->device,
                d->class_code, d->subclass, d->irq_line, 32 + d->irq_line);
    }
}

static void cmd_uname(void)
{
    kprintf("TorOS 0.1.0 (faz1-input) aarch64 QEMU-virt Cortex-A72\n");
    kprintf("Timer: %lu Hz, tick: %d Hz\n", timer_freq(), TIMER_HZ);
    kprintf("Input cihazları: %u\n", input_device_count());
}

static void execute(char *cmdline)
{
    /* argümanları ayır */
    while (*cmdline == ' ')
        cmdline++;
    if (!*cmdline)
        return;

    char *args = cmdline;
    while (*args && *args != ' ')
        args++;
    if (*args) {
        *args = 0;
        args++;
        while (*args == ' ')
            args++;
    }

    if (strcmp(cmdline, "help") == 0)        cmd_help();
    else if (strcmp(cmdline, "ls") == 0)     cmd_ls(args);
    else if (strcmp(cmdline, "cat") == 0)    cmd_cat(args);
    else if (strcmp(cmdline, "write") == 0)  cmd_write(args);
    else if (strcmp(cmdline, "rm") == 0)     cmd_rm(args);
    else if (strcmp(cmdline, "events") == 0) cmd_events(args);
    else if (strcmp(cmdline, "mouse") == 0)  cmd_mouse();
    else if (strcmp(cmdline, "kbdstat") == 0) cmd_kbdstat();
    else if (strcmp(cmdline, "mem") == 0)    cmd_mem();
    else if (strcmp(cmdline, "ps") == 0)     cmd_ps();
    else if (strcmp(cmdline, "pci") == 0)    cmd_pci();
    else if (strcmp(cmdline, "uptime") == 0)
        kprintf("%lu ms (%lu tick)\n", timer_uptime_ms(), timer_ticks());
    else if (strcmp(cmdline, "uname") == 0)  cmd_uname();
    else if (strcmp(cmdline, "clear") == 0)  kprintf("\x1b[2J\x1b[H");
    else
        kprintf("bilinmeyen komut: '%s' (help yazın)\n", cmdline);
}

static void handle_char(int c)
{
    if (c == '\r' || c == '\n') {
        kprintf("\n");
        line[line_len] = 0;
        execute(line);
        line_len = 0;
        prompt();
    } else if (c == '\b' || c == 0x7f) {
        if (line_len > 0) {
            line_len--;
            kprintf("\b \b");
        }
    } else if (c == 0x03) {
        /* Ctrl+C: satırı iptal */
        kprintf("^C\n");
        line_len = 0;
        prompt();
    } else if (c >= 0x20 && c < 0x7f && line_len < LINE_MAX - 1) {
        line[line_len++] = (char)c;
        uart_putc((char)c);
    }
}

void shell_run(void *arg)
{
    (void)arg;
    kprintf("%s", banner);
    kprintf("'help' yazarak başlayın. Girdi: UART + virtio klavye\n\n");
    prompt();

    for (;;) {
        int c = -1;
        if (uart_rx_ready()) {
            c = uart_getc();
        } else {
            char kc;
            if (keyboard_poll(&kc))
                c = (u8)kc;
        }
        if (c < 0) {
            wfe();
            continue;
        }
        handle_char(c);
    }
}

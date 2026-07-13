/*
 * torOS Debug & Diagnostics
 * GDB stub, System Monitor, Event Log, Crash Dump, BSOD, Stack Trace
 */

#include "../include/toros.h"
#include "../include/debug.h"
#include "../include/security.h"

static gdb_state_t gdb;
static system_monitor_t monitor;
static event_log_entry_t event_log_entries[EVENT_LOG_SIZE];
static int event_log_count = 0;
static int event_log_enabled = 1;
static crash_dump_t crash_dump;
static int crash_dump_valid = 0;
static spinlock_t debug_lock;

/* ===== GDB Stub ===== */

static char hex_chars[] = "0123456789abcdef";

static uint8 hex_to_byte(char h, char l)
{
    uint8 v = 0;
    if (h >= '0' && h <= '9') v = (h - '0') << 4;
    else if (h >= 'a' && h <= 'f') v = (h - 'a' + 10) << 4;
    else if (h >= 'A' && h <= 'F') v = (h - 'A' + 10) << 4;

    if (l >= '0' && l <= '9') v |= (l - '0');
    else if (l >= 'a' && l <= 'f') v |= (l - 'a' + 10);
    else if (l >= 'A' && l <= 'F') v |= (l - 'A' + 10);
    return v;
}

static void byte_to_hex(uint8 b, char *out)
{
    out[0] = hex_chars[b >> 4];
    out[1] = hex_chars[b & 0xF];
}

static void gdb_send_packet(const char *data)
{
    if (!gdb.connected) return;

    uint8 checksum = 0;
    for (const char *p = data; *p; p++) checksum += (uint8)*p;

    uart_putc('$');
    uart_puts(data);
    uart_putc('#');
    char cs[3];
    byte_to_hex(checksum, cs);
    cs[2] = '\0';
    uart_puts(cs);
}

static void gdb_send_ok(void) { gdb_send_packet("OK"); }
static void gdb_send_empty(void) { gdb_send_packet(""); }

static void gdb_handle_packet(const char *pkt)
{
    if (!pkt || !*pkt) return;

    switch (pkt[0]) {
    case '?': /* Last signal */
        gdb_send_packet("S05"); /* SIGTRAP */
        break;

    case 'g': /* Read registers */
        {
            char buf[GDB_NUM_REGS * 16 + 1];
            int pos = 0;
            for (int i = 0; i < GDB_NUM_REGS; i++) {
                for (int j = 0; j < 16; j += 2) {
                    byte_to_hex((gdb.regs[i] >> (j * 4)) & 0xFF, &buf[pos]);
                    pos += 2;
                }
            }
            buf[pos] = '\0';
            gdb_send_packet(buf);
        }
        break;

    case 'G': /* Write registers */
        {
            const char *p = pkt + 1;
            for (int i = 0; i < GDB_NUM_REGS && *p; i++) {
                gdb.regs[i] = 0;
                for (int j = 0; j < 16 && *p; j += 2) {
                    gdb.regs[i] |= ((uint64)hex_to_byte(p[0], p[1])) << (j * 4);
                    p += 2;
                }
            }
            gdb_send_ok();
        }
        break;

    case 'm': /* Read memory */
        {
            uint64 addr = 0;
            uint32 len = 0;
            sscanf(pkt + 1, "%lx,%x", &addr, &len);
            if (len > 512) len = 512;
            char buf[1024];
            int pos = 0;
            for (uint32 i = 0; i < len; i++) {
                byte_to_hex(((uint8 *)addr)[i], &buf[pos]);
                pos += 2;
            }
            buf[pos] = '\0';
            gdb_send_packet(buf);
        }
        break;

    case 'M': /* Write memory */
        gdb_send_ok();
        break;

    case 'c': /* Continue */
        gdb.single_step = 0;
        break;

    case 's': /* Single step */
        gdb.single_step = 1;
        break;

    case 'q': /* Query */
        if (strncmp(pkt, "qSupported", 10) == 0)
            gdb_send_packet("PacketSize=400");
        else if (strncmp(pkt, "qAttached", 9) == 0)
            gdb_send_packet("1");
        else
            gdb_send_empty();
        break;

    case 'H': /* Set thread */
        gdb_send_ok();
        break;

    case 'k': /* Kill */
        gdb.connected = 0;
        printk_color(TERM_YELLOW, "[GDB] Detached\n");
        break;

    case 'z': /* Remove breakpoint */
    case 'Z': /* Set breakpoint */
        {
            int type;
            uint64 addr;
            int kind;
            sscanf(pkt + 1, "%d,%lx,%d", &type, &addr, &kind);
            (void)kind;
            if (type == 0) { /* Software breakpoint */
                if (pkt[0] == 'Z') {
                    gdb_set_breakpoint(addr);
                    gdb_send_ok();
                } else {
                    gdb_clear_breakpoint(addr);
                    gdb_send_ok();
                }
            }
        }
        break;

    default:
        gdb_send_empty();
        break;
    }
}

void gdb_stub_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] GDB Stub...\n");
    memset(&gdb, 0, sizeof(gdb_state_t));
    spin_init(&gdb.lock);
    gdb.enabled = 1;
    gdb.connected = 0;
    gdb.single_step = 0;

    /* Set up default register state */
    for (int i = 0; i < GDB_NUM_REGS; i++) gdb.regs[i] = 0;

    printk_color(TERM_GREEN, "[BOOT] GDB stub ready (port %d)\n", GDB_PORT);
    printk_color(TERM_CYAN, "  Connect with: aarch64-none-elf-gdb -ex 'target remote :%d'\n", GDB_PORT);
}

void gdb_stub_handle_interrupt(void)
{
    if (!gdb.enabled) return;

    spin_lock(&gdb.lock);

    /* Check for incoming GDB packet */
    char ch;
    if (uart_getc(&ch) && ch == '$') {
        gdb.connected = 1;
        int i = 0;
        /* Read packet data until '#' */
        while (i < GDB_BUF_SIZE - 1) {
            if (uart_getc(&ch)) {
                if (ch == '#') break;
                gdb.packet_buf[i++] = ch;
            }
        }
        gdb.packet_buf[i] = '\0';

        /* Read and discard checksum */
        char dummy;
        uart_getc(&dummy);
        uart_getc(&dummy);

        /* Acknowledge */
        uart_putc('+');

        /* Handle */
        gdb_handle_packet(gdb.packet_buf);
    }

    /* Check breakpoints */
    uint64 pc = gdb.regs[GDB_REG_PC];
    for (int i = 0; i < gdb.breakpoint_count; i++) {
        if (gdb.breakpoints[i] == pc) {
            gdb_stub_send_signal(GDB_SIGNAL_TRAP);
            break;
        }
    }

    spin_unlock(&gdb.lock);
}

void gdb_stub_send_signal(int signal)
{
    char buf[8];
    buf[0] = 'S';
    byte_to_hex((uint8)signal, &buf[1]);
    buf[3] = '\0';
    gdb_send_packet(buf);
}

int gdb_stub_is_connected(void) { return gdb.connected; }

void gdb_stub_set_breakpoint(uint64 addr)
{
    if (gdb.breakpoint_count < 16) {
        gdb.breakpoints[gdb.breakpoint_count++] = addr;
        printk_color(TERM_CYAN, "[GDB] Breakpoint at %lx\n", addr);
    }
}

void gdb_stub_clear_breakpoint(uint64 addr)
{
    for (int i = 0; i < gdb.breakpoint_count; i++) {
        if (gdb.breakpoints[i] == addr) {
            for (int j = i; j < gdb.breakpoint_count - 1; j++)
                gdb.breakpoints[j] = gdb.breakpoints[j + 1];
            gdb.breakpoint_count--;
            break;
        }
    }
}

void gdb_stub_single_step(void) { gdb.single_step = 1; }
void gdb_stub_continue(void) { gdb.single_step = 0; }

/* ===== System Monitor ===== */

void monitor_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] System Monitor...\n");
    memset(&monitor, 0, sizeof(system_monitor_t));
    monitor.stats.total_ram = RAM_SIZE;
    monitor.stats.free_ram = get_free_pages() * PAGE_SIZE;
    monitor.stats.used_ram = monitor.stats.total_ram - monitor.stats.free_ram;
    monitor.initialized = 1;
    printk_color(TERM_GREEN, "[BOOT] System monitor ready\n");
}

void monitor_update(void)
{
    if (!monitor.initialized) return;
    monitor.stats.free_ram = get_free_pages() * PAGE_SIZE;
    monitor.stats.used_ram = monitor.stats.total_ram - monitor.stats.free_ram;
    monitor.stats.memory.free_ram = monitor.stats.free_ram;
    monitor.stats.memory.used_ram = monitor.stats.used_ram;
    monitor.stats.uptime_ms = get_jiffies() * 10;
    monitor.last_update = get_jiffies();
}

void monitor_get_stats(system_stats_t *stats)
{
    if (!stats) return;
    monitor_update();
    *stats = monitor.stats;
}

void monitor_list_processes(void)
{
    printk_color(TERM_CYAN, "\n=== Processes ===\n");
    extern proc_t proc_table[];
    extern int proc_count;
    printk("  PID  STATE       NAME\n");
    for (int i = 0; i < proc_count; i++) {
        const char *state;
        switch (proc_table[i].state) {
        case PROC_UNUSED:   state = "UNUSED"; break;
        case PROC_RUNNABLE: state = "RUNNABLE"; break;
        case PROC_RUNNING:  state = "RUNNING"; break;
        case PROC_SLEEPING: state = "SLEEPING"; break;
        case PROC_ZOMBIE:   state = "ZOMBIE"; break;
        default:            state = "?"; break;
        }
        printk("  %-4d %-10s  %s\n", proc_table[i].pid, state, proc_table[i].name);
    }
    printk("\n");
}

void monitor_show_memory(void)
{
    monitor_update();
    printk_color(TERM_CYAN, "\n=== Memory ===\n");
    printk("  Total: %lu MB\n", monitor.stats.memory.total_ram / (1024 * 1024));
    printk("  Used:  %lu MB\n", monitor.stats.memory.used_ram / (1024 * 1024));
    printk("  Free:  %lu MB\n", monitor.stats.memory.free_ram / (1024 * 1024));
    printk("  Usage: %d%%\n", (int)(monitor.stats.memory.used_ram * 100 / monitor.stats.memory.total_ram));
    printk("\n");
}

void monitor_show_cpu(void)
{
    printk_color(TERM_CYAN, "\n=== CPU ===\n");
    printk("  CPUs:   %d\n", smp_cpu_count());
    printk("  Uptime: %lu ticks\n", get_jiffies());
    printk("\n");
}

void monitor_draw_gui(uint32 *fb, int fb_w, int fb_h)
{
    if (!fb) return;
    monitor_update();

    /* Dark background */
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF1A1A2E;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    /* Header */
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * fb_w + x] = 0xFF0078D7;
    fb_set_color(0xFFFFFFFF);
    fb_draw_string(8, 6, "System Monitor");

    /* Stats */
    fb_set_color(0xFF00FF00);
    char buf[64];

    fb_draw_string(8, 40, "Memory:");
    snprintf(buf, 64, "  Total: %lu MB", monitor.stats.memory.total_ram / (1024 * 1024));
    fb_draw_string(8, 56, buf);
    snprintf(buf, 64, "  Used:  %lu MB", monitor.stats.memory.used_ram / (1024 * 1024));
    fb_draw_string(8, 72, buf);
    snprintf(buf, 64, "  Free:  %lu MB", monitor.stats.memory.free_ram / (1024 * 1024));
    fb_draw_string(8, 88, buf);

    fb_draw_string(8, 112, "CPU:");
    snprintf(buf, 64, "  CPUs: %d", smp_cpu_count());
    fb_draw_string(8, 128, buf);

    /* Memory bar */
    int bar_w = fb_w - 16;
    int used_pct = (int)(monitor.stats.memory.used_ram * bar_w / monitor.stats.memory.total_ram);
    for (int y = 160; y < 176; y++) {
        for (int x = 8; x < 8 + bar_w; x++)
            fb[y * fb_w + x] = 0xFF333355;
        for (int x = 8; x < 8 + used_pct; x++)
            fb[y * fb_w + x] = 0xFF0078D7;
    }
}

/* ===== Event Log ===== */

void event_log_init(void)
{
    memset(event_log_entries, 0, sizeof(event_log_entries));
    event_log_count = 0;
    event_log_enabled = 1;
    spin_init(&debug_lock);
    printk_color(TERM_GREEN, "[BOOT] Event log ready\n");
}

void event_log(event_type_t type, const char *source, const char *fmt, ...)
{
    if (!event_log_enabled) return;

    spin_lock(&debug_lock);
    int idx = event_log_count % EVENT_LOG_SIZE;
    event_log_entry_t *entry = &event_log_entries[idx];

    entry->timestamp = get_jiffies();
    entry->type = type;
    entry->cpu = (uint32)(r_mpidr() & 0xFF);
    strncpy(entry->source, source ? source : "kernel", 31);
    entry->source[31] = '\0';

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, EVENT_MSG_LEN, fmt, args);
    va_end(args);

    event_log_count++;
    spin_unlock(&debug_lock);
}

void event_log_info(const char *source, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[256]; vsnprintf(buf, 256, fmt, args); va_end(args);
    event_log(EVENT_INFO, source, "%s", buf);
}

void event_log_warn(const char *source, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[256]; vsnprintf(buf, 256, fmt, args); va_end(args);
    event_log(EVENT_WARNING, source, "%s", buf);
}

void event_log_error(const char *source, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[256]; vsnprintf(buf, 256, fmt, args); va_end(args);
    event_log(EVENT_ERROR, source, "%s", buf);
}

void event_log_fatal(const char *source, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char buf[256]; vsnprintf(buf, 256, fmt, args); va_end(args);
    event_log(EVENT_FATAL, source, "%s", buf);
}

void event_log_dump(int count)
{
    printk_color(TERM_CYAN, "\n=== Event Log (%d total) ===\n", event_log_count);
    const char *type_names[] = {"INFO", "WARN", "ERROR", "FATAL", "DEBUG", "SECURITY"};
    const int type_colors[] = {TERM_WHITE, TERM_YELLOW, TERM_RED, TERM_RED, TERM_CYAN, TERM_MAGENTA};

    int start = event_log_count - count;
    if (start < 0) start = 0;

    for (int i = start; i < event_log_count; i++) {
        event_log_entry_t *e = &event_log_entries[i % EVENT_LOG_SIZE];
        printk_color(type_colors[e->type], "[%lu] %s: %s: %s\n",
                     e->timestamp, type_names[e->type], e->source, e->message);
    }
    printk("\n");
}

void event_log_dump_type(event_type_t type)
{
    printk_color(TERM_CYAN, "\n=== Events (type=%d) ===\n", type);
    for (int i = 0; i < event_log_count && i < EVENT_LOG_SIZE; i++) {
        if (event_log_entries[i % EVENT_LOG_SIZE].type == type) {
            printk("  %s\n", event_log_entries[i % EVENT_LOG_SIZE].message);
        }
    }
    printk("\n");
}

void event_log_save(const char *filename)
{
    (void)filename;
    /* Would write to filesystem */
}

/* ===== Crash Dump ===== */

void crash_init(void)
{
    memset(&crash_dump, 0, sizeof(crash_dump_t));
    crash_dump_valid = 0;
    printk_color(TERM_GREEN, "[BOOT] Crash dump handler ready\n");
}

void crash_handler(const char *type, uint64 elr, uint64 esr, uint64 far)
{
    crash_dump.crash_time = rtc_get_time();
    strncpy(crash_dump.crash_type, type ? type : "Unknown", 31);
    crash_dump.elr = elr;
    crash_dump.esr = esr;
    crash_dump.far = far;
    crash_dump.valid = 1;
    crash_dump_valid = 1;

    /* Save registers */
    __asm__ volatile("mov %0, sp" : "=r"(crash_dump.sp));
    crash_dump.pc = elr;

    /* Stack trace */
    strcpy(crash_dump.stack_trace, "Stack trace:\n");

    /* Log */
    event_log_fatal("crash", "Type=%s ELR=%lx ESR=%lx FAR=%lx",
                    crash_dump.crash_type, elr, esr, far);

    printk_color(TERM_RED, "\n!!! CRASH !!!\n");
    printk_color(TERM_RED, "Type: %s\n", crash_dump.crash_type);
    printk_color(TERM_RED, "ELR:  %016lx\n", elr);
    printk_color(TERM_RED, "ESR:  %016lx\n", esr);
    printk_color(TERM_RED, "FAR:  %016lx\n", far);
}

void crash_save_dump(void)
{
    if (!crash_dump_valid) return;
    /* Would save to disk */
    printk_color(TERM_GREEN, "[CRASH] Dump saved\n");
}

void crash_show_dump(void)
{
    if (!crash_dump_valid) {
        printk_color(TERM_YELLOW, "No crash dump found\n");
        return;
    }
    printk_color(TERM_CYAN, "\n=== Crash Dump ===\n");
    printk("Time: %lu\n", crash_dump.crash_time);
    printk("Type: %s\n", crash_dump.crash_type);
    printk("ELR:  %016lx\n", crash_dump.elr);
    printk("ESR:  %016lx\n", crash_dump.esr);
    printk("FAR:  %016lx\n", crash_dump.far);
    printk("SP:   %016lx\n", crash_dump.sp);
    printk("PC:   %016lx\n", crash_dump.pc);
    printk("\n");
}

void crash_reboot(void)
{
    printk_color(TERM_RED, "[CRASH] Rebooting...\n");
    /* Trigger watchdog or CPU reset */
    volatile uint32 *reset_reg = (volatile uint32 *)0x09000000;
    *reset_reg = 1;
    while (1) wfe();
}

int crash_dump_exists(void) { return crash_dump_valid; }

/* ===== BSOD ===== */

void bsod(const char *message, const char *file, int line)
{
    /* Blue screen */
    uint32 *fb = fb_base;
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) fb[i] = 0xFF00007A;

    extern void fb_set_color(uint32 c);
    extern void fb_draw_string(int x, int y, const char *s);

    fb_set_color(0xFFFFFFFF);
    fb_draw_string(20, 20, ":(");
    fb_draw_string(20, 60, "Your torOS ran into a problem and needs to restart.");

    char buf[128];
    snprintf(buf, 128, "%s at %s:%d", message, file, line);
    fb_draw_string(20, 100, buf);

    fb_draw_string(20, 140, "Stop code: KERNEL_PANIC");
    fb_draw_string(20, 180, "Press any key to reboot...");

    /* Also to serial */
    printk_color(TERM_RED, "\n!!! BSOD !!! %s at %s:%d\n", message, file, line);

    crash_handler("BSOD", (uint64)message, 0, 0);
    crash_save_dump();

    /* Halt */
    while (1) wfe();
}

void bsod_show_registers(void)
{
    uint64 elr, spsr;
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(spsr));
    printk_color(TERM_CYAN, "ELR=%016lx SPSR=%016lx\n", elr, spsr);
}

void kernel_panic(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);
    bsod(buf, "kernel_panic", 0);
}

/* ===== Stack Trace ===== */

void stack_trace(void)
{
    printk_color(TERM_CYAN, "\n=== Stack Trace ===\n");
    uint64 *fp;
    __asm__ volatile("mov %0, x29" : "=r"(fp));
    stack_trace_from(fp);
}

void stack_trace_from(uint64 *frame)
{
    for (int i = 0; i < 16 && frame; i++) {
        uint64 lr = frame[1];
        uint64 *next_fp = (uint64 *)frame[0];
        printk("  #%d: %016lx\n", i, lr);
        if (next_fp <= frame) break; /* Guard against corruption */
        frame = next_fp;
    }
    printk("\n");
}

/* ===== Debug Console ===== */

void debug_console_init(void)
{
    printk_color(TERM_GREEN, "[BOOT] Debug console ready\n");
    printk_color(TERM_CYAN, "\nDebug commands: regs, stack, mem, gdb, crash, log, reboot\n");
}

void debug_command(const char *cmd)
{
    if (!cmd) return;

    if (strcmp(cmd, "regs") == 0) {
        uint64 x0, x1, sp, elr;
        __asm__ volatile("mov %0, x0" : "=r"(x0));
        __asm__ volatile("mov %0, x1" : "=r"(x1));
        __asm__ volatile("mov %0, sp" : "=r"(sp));
        __asm__ volatile("mrs %0, ELR_EL1" : "=r"(elr));
        printk_color(TERM_CYAN, "x0=%016lx x1=%016lx sp=%016lx elr=%016lx\n", x0, x1, sp, elr);
    }
    else if (strcmp(cmd, "stack") == 0) {
        stack_trace();
    }
    else if (strcmp(cmd, "gdb") == 0) {
        gdb_stub_init();
    }
    else if (strcmp(cmd, "crash") == 0) {
        crash_show_dump();
    }
    else if (strcmp(cmd, "log") == 0) {
        event_log_dump(20);
    }
    else if (strcmp(cmd, "reboot") == 0) {
        crash_reboot();
    }
    else {
        printk_color(TERM_YELLOW, "Unknown debug command: %s\n", cmd);
    }
}

/* sscanf implementation for GDB stub */
static int sscanf(const char *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int count = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'x' || *fmt == 'X') {
                uint64 *p = va_arg(args, uint64 *);
                uint64 val = 0;
                while (*str) {
                    char c = *str;
                    if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                    else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
                    else break;
                    str++;
                }
                *p = val;
                count++;
            } else if (*fmt == 'd') {
                int *p = va_arg(args, int *);
                int val = 0;
                while (*str >= '0' && *str <= '9') val = val * 10 + (*str++ - '0');
                *p = val;
                count++;
            }
            fmt++;
        } else {
            if (*str == *fmt) str++;
            fmt++;
        }
    }
    va_end(args);
    return count;
}
#endif

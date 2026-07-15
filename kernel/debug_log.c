/******************************************************************************
 * torOS - Terminal Operating System
 * Debug Logging Subsystem - Structured log with levels, filtering, storage
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/debug.h"
#include "../include/security.h"

/* ===== Log Buffers ===== */

static log_entry_t log_ring[LOG_RING_SIZE];
static volatile uint32 log_write_idx = 0;
static volatile uint32 log_read_idx = 0;
static int log_initialized = 0;
static spinlock_t log_lock;
static uint32 log_filter_level = LOG_LEVEL_DEBUG;
static int log_to_serial = 1;
static int log_to_framebuffer = 0;
static uint32 log_drop_count = 0;

/* Log level names */
static const char *level_names[] = {
    "EMERG", "ALERT", "CRIT", "ERROR",
    "WARN", "NOTICE", "INFO", "DEBUG"
};
static const int level_colors[] = {
    TERM_RED, TERM_RED, TERM_RED, TERM_RED,
    TERM_YELLOW, TERM_GREEN, TERM_WHITE, TERM_CYAN
};

/* ===== Initialization ===== */

void log_init(void)
{
    memset(log_ring, 0, sizeof(log_ring));
    log_write_idx = 0;
    log_read_idx = 0;
    log_drop_count = 0;
    log_filter_level = LOG_LEVEL_DEBUG;
    log_to_serial = 1;
    log_to_framebuffer = 0;
    spin_init(&log_lock);
    log_initialized = 1;
    printk_color(TERM_GREEN, "[BOOT] Debug log initialized\n");
}

void log_set_level(uint32 level)
{
    if (level <= LOG_LEVEL_DEBUG)
        log_filter_level = level;
}

uint32 log_get_level(void)
{
    return log_filter_level;
}

void log_set_serial_output(int enable)
{
    log_to_serial = enable;
}

void log_set_fb_output(int enable)
{
    log_to_framebuffer = enable;
}

/* ===== Core Logging ===== */

void log_write(uint32 level, const char *subsystem, const char *fmt, ...)
{
    if (!log_initialized) return;
    if (level > log_filter_level) return;
    if (!subsystem || !fmt) return;

    spin_lock(&log_lock);

    uint32 idx = log_write_idx % LOG_RING_SIZE;
    log_entry_t *entry = &log_ring[idx];

    entry->timestamp = rtc_get_time();
    entry->level = level;
    entry->cpu_id = (uint32)(r_mpidr() & 0xFF);
    strncpy(entry->subsystem, subsystem, LOG_SUBSYS_LEN - 1);
    entry->subsystem[LOG_SUBSYS_LEN - 1] = '\0';

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, LOG_MSG_LEN, fmt, args);
    va_end(args);

    log_write_idx++;

    /* Check for overflow */
    if (log_write_idx - log_read_idx > LOG_RING_SIZE) {
        log_read_idx = log_write_idx - LOG_RING_SIZE;
        log_drop_count++;
    }

    spin_unlock(&log_lock);

    /* Output to serial */
    if (log_to_serial) {
        printk_color(level_colors[level], "[%s][%s] %s\n",
                     level_names[level], subsystem, entry->message);
    }
}

void log_writev(uint32 level, const char *subsystem, const char *fmt, __builtin_va_list args)
{
    if (!log_initialized) return;
    if (level > log_filter_level) return;

    char buf[LOG_MSG_LEN];
    vsnprintf(buf, LOG_MSG_LEN, fmt, args);
    log_write(level, subsystem, "%s", buf);
}

/* ===== Convenience Functions ===== */

void log_emerg(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_EMERG, subsystem, fmt, args);
    va_end(args);
}

void log_alert(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_ALERT, subsystem, fmt, args);
    va_end(args);
}

void log_crit(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_CRIT, subsystem, fmt, args);
    va_end(args);
}

void log_error(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_ERROR, subsystem, fmt, args);
    va_end(args);
}

void log_warn(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_WARN, subsystem, fmt, args);
    va_end(args);
}

void log_notice(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_NOTICE, subsystem, fmt, args);
    va_end(args);
}

void log_info(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_INFO, subsystem, fmt, args);
    va_end(args);
}

void log_debug(const char *subsystem, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_writev(LOG_LEVEL_DEBUG, subsystem, fmt, args);
    va_end(args);
}

/* ===== Log Query ===== */

uint32 log_count(void)
{
    return log_write_idx - log_read_idx;
}

uint32 log_dropped(void)
{
    return log_drop_count;
}

const log_entry_t *log_get_entry(uint32 index)
{
    if (!log_initialized) return NULL;
    uint32 actual = log_read_idx + index;
    if (actual >= log_write_idx) return NULL;
    return &log_ring[actual % LOG_RING_SIZE];
}

void log_dump_all(void)
{
    if (!log_initialized) return;
    uint32 count = log_count();
    printk_color(TERM_CYAN, "\n=== System Log (%u entries, %u dropped) ===\n",
                 count, log_drop_count);

    for (uint32 i = 0; i < count && i < LOG_RING_SIZE; i++) {
        const log_entry_t *e = log_get_entry(i);
        if (!e) break;
        printk_color(level_colors[e->level], "[%08lu][%s][CPU%d] %s\n",
                     e->timestamp, level_names[e->level],
                     e->cpu_id, e->message);
    }
    printk("\n");
}

void log_dump_filtered(uint32 min_level, const char *subsystem)
{
    if (!log_initialized) return;
    uint32 count = log_count();
    printk_color(TERM_CYAN, "\n=== Filtered Log (level>=%u", min_level);
    if (subsystem) printk(", subsystem=%s", subsystem);
    printk(") ===\n");

    for (uint32 i = 0; i < count && i < LOG_RING_SIZE; i++) {
        const log_entry_t *e = log_get_entry(i);
        if (!e) break;
        if (e->level < min_level) continue;
        if (subsystem && strcmp(e->subsystem, subsystem) != 0) continue;
        printk_color(level_colors[e->level], "[%08lu][%s] %s: %s\n",
                     e->timestamp, level_names[e->level],
                     e->subsystem, e->message);
    }
    printk("\n");
}

void log_clear(void)
{
    if (!log_initialized) return;
    spin_lock(&log_lock);
    log_read_idx = log_write_idx;
    log_drop_count = 0;
    spin_unlock(&log_lock);
}

/* ===== Log Save/Load ===== */

int log_save_to_file(const char *filename)
{
    if (!log_initialized || !filename) return -1;

    int fd = tfs_create(filename);
    if (fd < 0) return -1;

    uint32 count = log_count();
    char buf[256];

    for (uint32 i = 0; i < count && i < LOG_RING_SIZE; i++) {
        const log_entry_t *e = log_get_entry(i);
        if (!e) break;
        snprintf(buf, sizeof(buf), "[%lu][%s][%s] %s\n",
                 e->timestamp, level_names[e->level],
                 e->subsystem, e->message);
        tfs_write(filename, buf, strlen(buf), i * 256);
    }
    return 0;
}

/* ===== Kernel Integration ===== */

void log_kernel_boot(void)
{
    log_info("kernel", "torOS v%s booting", TOROS_VER);
    log_info("kernel", "Architecture: ARM64 (AArch64)");
    log_info("kernel", "CPU: %d cores", smp_cpu_count());
}

void log_panic(const char *reason)
{
    log_emerg("kernel", "PANIC: %s", reason);
    log_dump_all();
}

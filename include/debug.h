/*
 * torOS Debug & Diagnostics Header
 * Kernel debugger (GDB stub), System Monitor, Event Log, Crash Dump
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include "toros.h"

/* ===== GDB Stub ===== */
#define GDB_BUF_SIZE        2048
#define GDB_NUM_REGS        34
#define GDB_SIGNAL_TRAP     5
#define GDB_SIGNAL_ABORT    6
#define GDB_SIGNAL_BUS      10
#define GDB_SIGNAL_SEGV     11

#define GDB_PORT            1234

/* Register indices */
#define GDB_REG_X0          0
#define GDB_REG_X30         30
#define GDB_REG_SP          31
#define GDB_REG_PC          32
#define GDB_REG_CPSR        33

typedef struct {
    uint64 regs[GDB_NUM_REGS];
    int enabled;
    int connected;
    int single_step;
    int breakpoint_count;
    uint64 breakpoints[16];
    char packet_buf[GDB_BUF_SIZE];
    int packet_len;
    spinlock_t lock;
} gdb_state_t;

/* ===== System Monitor ===== */
#define MON_MAX_PROCESSES   64
#define MON_HISTORY_SIZE    100

typedef struct {
    uint64 timestamp;
    uint32 pid;
    char name[NAMELEN];
    proc_state_t state;
    uint64 cpu_time;
    uint64 memory_used;
    uint32 priority;
} proc_info_t;

typedef struct {
    uint64 total_ram;
    uint64 free_ram;
    uint64 used_ram;
    uint64 cached_ram;
    uint64 total_swap;
    uint64 free_swap;
} memory_info_t;

typedef struct {
    uint64 uptime_ms;
    uint32 cpu_percent;
    uint32 num_processes;
    uint32 num_threads;
    uint64 ctx_switches;
    uint64 interrupts;
    memory_info_t memory;
} system_stats_t;

typedef struct {
    proc_info_t processes[MON_MAX_PROCESSES];
    int proc_count;
    system_stats_t stats;
    uint64 last_update;
    int initialized;
} system_monitor_t;

/* ===== Event Log ===== */
#define EVENT_LOG_SIZE      512
#define EVENT_MSG_LEN       256

typedef enum {
    EVENT_INFO,
    EVENT_WARNING,
    EVENT_ERROR,
    EVENT_FATAL,
    EVENT_DEBUG,
    EVENT_SECURITY
} event_type_t;

typedef struct {
    uint64 timestamp;
    event_type_t type;
    char source[32];
    char message[EVENT_MSG_LEN];
    uint32 cpu;
} event_log_entry_t;

/* ===== Crash Dump ===== */
#define CRASH_DUMP_SIZE     (1024 * 1024)

typedef struct {
    uint64 crash_time;
    char crash_type[32];
    uint64 fault_addr;
    uint64 elr;
    uint64 spsr;
    uint64 esr;
    uint64 far;
    uint64 sp;
    uint64 xregs[31];
    uint64 pc;
    char stack_trace[1024];
    char module_info[256];
    uint8 valid;
} crash_dump_t;

/* ===== Debug API ===== */
void gdb_stub_init(void);
void gdb_stub_handle_interrupt(void);
void gdb_stub_send_signal(int signal);
int gdb_stub_is_connected(void);
void gdb_stub_set_breakpoint(uint64 addr);
void gdb_stub_clear_breakpoint(uint64 addr);
void gdb_stub_single_step(void);
void gdb_stub_continue(void);

/* System Monitor */
void monitor_init(void);
void monitor_update(void);
void monitor_get_stats(system_stats_t *stats);
void monitor_list_processes(void);
void monitor_show_memory(void);
void monitor_show_cpu(void);
void monitor_draw_gui(uint32 *fb, int fb_w, int fb_h);

/* Event Log */
void event_log_init(void);
void event_log(event_type_t type, const char *source, const char *fmt, ...);
void event_log_info(const char *source, const char *fmt, ...);
void event_log_warn(const char *source, const char *fmt, ...);
void event_log_error(const char *source, const char *fmt, ...);
void event_log_fatal(const char *source, const char *fmt, ...);
void event_log_dump(int count);
void event_log_dump_type(event_type_t type);
void event_log_save(const char *filename);

/* Crash Dump */
void crash_init(void);
void crash_handler(const char *type, uint64 elr, uint64 esr, uint64 far);
void crash_save_dump(void);
void crash_show_dump(void);
void crash_reboot(void);
int crash_dump_exists(void);

/* Kernel Panic / BSOD */
void bsod(const char *message, const char *file, int line);
void bsod_show_registers(void);
void kernel_panic(const char *fmt, ...);

/* Stack trace */
void stack_trace(void);
void stack_trace_from(uint64 *frame);

/* Debug console */
void debug_console_init(void);
void debug_console_run(void);
void debug_command(const char *cmd);

#endif

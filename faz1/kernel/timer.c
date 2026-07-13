/*
 * timer.c — ARM Generic Timer (CNTP, PPI 30)
 */
#include <toros/timer.h>
#include <toros/gic.h>
#include <toros/irq.h>
#include <toros/printf.h>

static u64 freq;
static u64 ticks;
static u64 tval_reload;

static inline u64 read_cntfrq(void)
{
    u64 v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline u64 read_cntpct(void)
{
    u64 v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

static inline void write_cntp_tval(u64 v)
{
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(v));
}

static inline void write_cntp_ctl(u64 v)
{
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(v));
}

static void timer_irq(u32 intid, void *arg)
{
    (void)intid; (void)arg;
    /* Tekrar kur (one-shot TVAL modu) */
    write_cntp_tval(tval_reload);
    ticks++;
    /* Bekleyen task'ları/event-loop'ları uyandır */
    sev();
}

void timer_init(void)
{
    freq = read_cntfrq();
    if (freq == 0)
        freq = 62500000;   /* QEMU varsayılanı */
    tval_reload = freq / TIMER_HZ;

    write_cntp_ctl(0);             /* durdur + maskele */
    write_cntp_tval(tval_reload);

    irq_register(TIMER_INTID, timer_irq, NULL);
    gic_enable_irq(TIMER_INTID);

    write_cntp_ctl(1);             /* enable, maskesiz */

    kok("Timer: %lu Hz kaynak, %d Hz tick (PPI %d)\n",
        freq, TIMER_HZ, TIMER_INTID);
}

u64 timer_ticks(void)    { return ticks; }
u64 timer_freq(void)     { return freq; }
u64 timer_counter(void)  { return read_cntpct(); }
u64 timer_uptime_ms(void){ return (ticks * 1000) / TIMER_HZ; }

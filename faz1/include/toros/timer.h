/*
 * timer.h — ARM Generic Timer (CNTP, PPI 30)
 */
#ifndef TOROS_TIMER_H
#define TOROS_TIMER_H

#include <toros/types.h>

#define TIMER_HZ 100          /* tick frekansı: 100 Hz (10 ms) */
#define TIMER_INTID 30        /* fiziksel timer PPI */

void timer_init(void);
u64  timer_ticks(void);       /* başlangıçtan itibaren tick sayısı */
u64  timer_freq(void);        /* CNTFRQ (Hz) */
u64  timer_counter(void);     /* CNTPCT ham sayaç */
u64  timer_uptime_ms(void);

#endif

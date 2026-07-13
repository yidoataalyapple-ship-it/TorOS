/*
 * torOS RTC Driver
 * ARM64 Generic Timer timekeeping
 */

#include "../include/toros.h"

#define EPOCH_YEAR      2024
#define SECS_PER_MIN    60
#define SECS_PER_HOUR   3600
#define SECS_PER_DAY    86400

static const int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static uint64 boot_jiffies = 0;
static uint64 timer_freq = 0;

typedef struct {
    uint16 year; uint8 month, day, hour, minute, second;
} datetime_t;

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
static int days_in_month(int y, int m) { return (m == 2 && is_leap(y)) ? 29 : month_days[m - 1]; }

uint64 rtc_get_time(void)
{
    return ((get_jiffies() - boot_jiffies) * 10) / 1000;
}

static void seconds_to_dt(uint64 seconds, datetime_t *dt)
{
    uint64 rem = seconds;
    dt->year = EPOCH_YEAR;
    while (1) { uint64 s = (is_leap(dt->year) ? 366 : 365) * SECS_PER_DAY; if (rem < s) break; rem -= s; dt->year++; }
    dt->month = 1;
    while (1) { uint64 s = days_in_month(dt->year, dt->month) * SECS_PER_DAY; if (rem < s) break; rem -= s; dt->month++; }
    dt->day = 1 + rem / SECS_PER_DAY; rem %= SECS_PER_DAY;
    dt->hour = rem / SECS_PER_HOUR; rem %= SECS_PER_HOUR;
    dt->minute = rem / SECS_PER_MIN;
    dt->second = rem % SECS_PER_MIN;
}

void rtc_time_string(char *buf, int max_len)
{
    datetime_t dt;
    seconds_to_dt(rtc_get_time(), &dt);
    char *p = buf;
    utoa(dt.year, p, 10); p += strlen(p); *p++ = '-';
    if (dt.month < 10) *p++ = '0'; utoa(dt.month, p, 10); p += strlen(p); *p++ = '-';
    if (dt.day < 10) *p++ = '0'; utoa(dt.day, p, 10); p += strlen(p); *p++ = ' ';
    if (dt.hour < 10) *p++ = '0'; utoa(dt.hour, p, 10); p += strlen(p); *p++ = ':';
    if (dt.minute < 10) *p++ = '0'; utoa(dt.minute, p, 10); p += strlen(p); *p++ = ':';
    if (dt.second < 10) *p++ = '0'; utoa(dt.second, p, 10); p += strlen(p);
    *p = '\0';
    (void)max_len;
}

void rtc_print_time(void)
{
    char buf[32];
    rtc_time_string(buf, sizeof(buf));
    printk_color(TERM_CYAN, "  Time: %s\n", buf);
}

void rtc_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] RTC...\n");
    boot_jiffies = get_jiffies();
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(timer_freq));
    printk_color(TERM_GREEN, "[BOOT] RTC ready (%d Hz)\n", timer_freq);
}

uint64 rtc_get_ticks(void)
{
    uint64 t;
    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(t));
    return t;
}

void rtc_udelay(uint32 us)
{
    uint64 start = rtc_get_ticks();
    uint64 need = ((uint64)us * timer_freq) / 1000000;
    while ((rtc_get_ticks() - start) < need) ;
}

void rtc_mdelay(uint32 ms) { rtc_udelay(ms * 1000); }

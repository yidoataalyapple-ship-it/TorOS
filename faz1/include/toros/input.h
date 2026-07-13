/*
 * input.h — Birleşik input event altyapısı (Faz 1.4)
 * Linux input_event uyumlu olay yapısı + /dev/input/eventN kaydı
 */
#ifndef TOROS_INPUT_H
#define TOROS_INPUT_H

#include <toros/types.h>
#include <toros/spinlock.h>

/* Event tipleri (Linux ile uyumlu) */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_MSC 0x04
#define EV_LED 0x11

/* Relative axes */
#define REL_X     0
#define REL_Y     1
#define REL_WHEEL 8

/* Mouse butonları */
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/* Plan: struct input_event — /dev/input/eventN binary formatı */
struct input_event {
    u64 time_sec;
    u64 time_usec;
    u16 type;
    u16 code;
    s32 value;
};

#define INPUT_EVENT_RING_SIZE 64   /* Plan: 64-event ring buffer */
#define INPUT_MAX_DEVICES 8

struct input_dev {
    char name[64];
    u32  id;                       /* /dev/input/eventN -> N */
    /* Yetenek bitmapleri */
    u8   evbit[8];                 /* EV_* bitmap (64 tip) */
    u8   keybit[96];               /* key codes 0..767 */
    u8   relbit[2];                /* REL_* 0..15 */
    u8   absbit[8];                /* ABS_* 0..63 */
    /* Olay halkası */
    struct input_event ring[INPUT_EVENT_RING_SIZE];
    u32  head, tail;
    spinlock_t lock;
    u64  events_total;
    u64  events_dropped;
};

/* Alt sistem */
void input_init(void);
struct input_dev *input_register_device(const char *name);
void input_report_event(struct input_dev *dev, u16 type, u16 code, s32 value);
void input_report_key(struct input_dev *dev, u16 code, s32 value);
void input_report_rel(struct input_dev *dev, u16 code, s32 value);
void input_sync(struct input_dev *dev);

/* Okuma (devfs /dev/input/eventN tarafından kullanılır) */
int  input_read_event(struct input_dev *dev, struct input_event *ev);      /* non-block: 0=ok */
int  input_read_event_wait(struct input_dev *dev, struct input_event *ev); /* blocking */
int  input_read_any(struct input_event *ev, u64 timeout_ms);               /* tüm cihazlar */

struct input_dev *input_get_device(u32 id);
u32  input_device_count(void);

/* Bitmap yardımcıları */
static inline void input_set_bit(u8 *bitmap, u32 bit)
{
    bitmap[bit / 8] |= (u8)(1u << (bit % 8));
}

static inline int input_test_bit(const u8 *bitmap, u32 bit)
{
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}

#endif

/*
 * event.c — Birleşik input event altyapısı (Faz 1.4)
 *
 * Her kayıtlı cihaz /dev/input/eventN olarak torFS'e eklenir.
 * Olaylar 64-slot ring buffer'da tutulur (plan: 64-event ring buffer).
 */
#include <toros/input.h>
#include <toros/timer.h>
#include <toros/printf.h>
#include <toros/string.h>
#include <toros/torfs.h>

static struct input_dev devices[INPUT_MAX_DEVICES];
static u32 ndevices;

void input_init(void)
{
    memset(devices, 0, sizeof(devices));
    ndevices = 0;
}

struct input_dev *input_register_device(const char *name)
{
    if (ndevices >= INPUT_MAX_DEVICES) {
        kerr("input: cihaz tablosu dolu\n");
        return NULL;
    }

    struct input_dev *d = &devices[ndevices];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, sizeof(d->name) - 1);
    d->id = ndevices;
    spinlock_init(&d->lock);
    ndevices++;

    kinfo("input: cihaz kaydedildi: /dev/input/event%u (%s)\n", d->id, d->name);
    return d;
}

void input_report_event(struct input_dev *d, u16 type, u16 code, s32 value)
{
    if (!d)
        return;

    u64 ms = timer_uptime_ms();

    u64 flags = spin_lock_irqsave(&d->lock);
    u32 next = (d->head + 1) % INPUT_EVENT_RING_SIZE;
    if (next == d->tail) {
        /* Dolu: en eskiyi at */
        d->tail = (d->tail + 1) % INPUT_EVENT_RING_SIZE;
        d->events_dropped++;
    }
    struct input_event *ev = &d->ring[d->head];
    ev->time_sec = ms / 1000;
    ev->time_usec = (ms % 1000) * 1000;
    ev->type = type;
    ev->code = code;
    ev->value = value;
    d->head = next;
    d->events_total++;
    spin_unlock_irqrestore(&d->lock, flags);

    sev();  /* WFE'de bekleyen okuyucuları uyandır */
}

void input_report_key(struct input_dev *d, u16 code, s32 value)
{
    input_report_event(d, EV_KEY, code, value);
}

void input_report_rel(struct input_dev *d, u16 code, s32 value)
{
    input_report_event(d, EV_REL, code, value);
}

void input_sync(struct input_dev *d)
{
    input_report_event(d, EV_SYN, 0, 0);
}

int input_read_event(struct input_dev *d, struct input_event *ev)
{
    if (!d)
        return -1;
    u64 flags = spin_lock_irqsave(&d->lock);
    if (d->head == d->tail) {
        spin_unlock_irqrestore(&d->lock, flags);
        return -1;
    }
    *ev = d->ring[d->tail];
    d->tail = (d->tail + 1) % INPUT_EVENT_RING_SIZE;
    spin_unlock_irqrestore(&d->lock, flags);
    return 0;
}

int input_read_event_wait(struct input_dev *d, struct input_event *ev)
{
    while (input_read_event(d, ev) != 0)
        wfe();
    return 0;
}

int input_read_any(struct input_event *ev, u64 timeout_ms)
{
    u64 deadline = timer_ticks() + (timeout_ms * TIMER_HZ) / 1000 + 1;
    for (;;) {
        for (u32 i = 0; i < ndevices; i++) {
            if (input_read_event(&devices[i], ev) == 0)
                return (int)i;
        }
        if (timer_ticks() >= deadline)
            return -1;
        wfe();
    }
}

struct input_dev *input_get_device(u32 id)
{
    if (id >= ndevices)
        return NULL;
    return &devices[id];
}

u32 input_device_count(void)
{
    return ndevices;
}

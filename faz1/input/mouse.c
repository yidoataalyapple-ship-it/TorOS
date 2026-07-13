/*
 * mouse.c — Fare sürücüsü (Faz 1.2)
 *
 * EV_REL -> delta biriktirme + ekran sınırına clamp (0..1023, 0..767)
 * EV_KEY (BTN_*) -> buton bitmap
 */
#include <toros/mouse.h>
#include <toros/input.h>
#include <toros/printf.h>
#include <toros/string.h>

static struct input_dev *mouse_dev;

static s32 pos_x = MOUSE_SCREEN_W / 2;
static s32 pos_y = MOUSE_SCREEN_H / 2;
static u8  buttons;
static s32 wheel_acc;
static u64 ev_count;
static spinlock_t ms_lock = SPINLOCK_INIT;

void mouse_init(void)
{
    mouse_dev = input_register_device("TorOS Mouse");
    if (!mouse_dev)
        return;
    input_set_bit(mouse_dev->evbit, EV_KEY);
    input_set_bit(mouse_dev->evbit, EV_REL);
    input_set_bit(mouse_dev->relbit, REL_X);
    input_set_bit(mouse_dev->relbit, REL_Y);
    input_set_bit(mouse_dev->relbit, REL_WHEEL);
    input_set_bit(mouse_dev->keybit, BTN_LEFT);
    input_set_bit(mouse_dev->keybit, BTN_RIGHT);
    input_set_bit(mouse_dev->keybit, BTN_MIDDLE);
    kinfo("mouse: hazır (event%u), başlangıç (%d,%d)\n",
          mouse_dev->id, pos_x, pos_y);
}

/* virtio_input sürücüsünden çağrılır */
void mouse_handle_rel(u16 code, s32 value)
{
    u64 flags = spin_lock_irqsave(&ms_lock);
    switch (code) {
    case REL_X:
        pos_x += value;
        if (pos_x < 0) pos_x = 0;
        if (pos_x >= MOUSE_SCREEN_W) pos_x = MOUSE_SCREEN_W - 1;
        break;
    case REL_Y:
        pos_y += value;
        if (pos_y < 0) pos_y = 0;
        if (pos_y >= MOUSE_SCREEN_H) pos_y = MOUSE_SCREEN_H - 1;
        break;
    case REL_WHEEL:
        wheel_acc += value;
        break;
    default:
        break;
    }
    ev_count++;
    spin_unlock_irqrestore(&ms_lock, flags);

    input_report_rel(mouse_dev, code, value);
    input_sync(mouse_dev);
}

void mouse_handle_button(u16 code, s32 value)
{
    u8 bit = 0;
    switch (code) {
    case BTN_LEFT:   bit = BIT(0); break;
    case BTN_RIGHT:  bit = BIT(1); break;
    case BTN_MIDDLE: bit = BIT(2); break;
    default:
        return;
    }

    u64 flags = spin_lock_irqsave(&ms_lock);
    if (value)
        buttons |= bit;
    else
        buttons &= ~bit;
    ev_count++;
    spin_unlock_irqrestore(&ms_lock, flags);

    input_report_key(mouse_dev, code, value);
    input_sync(mouse_dev);
}

void mouse_get_state(s32 *x, s32 *y, u8 *btn)
{
    u64 flags = spin_lock_irqsave(&ms_lock);
    if (x) *x = pos_x;
    if (y) *y = pos_y;
    if (btn) *btn = buttons;
    spin_unlock_irqrestore(&ms_lock, flags);
}

s32 mouse_wheel_delta(void)
{
    u64 flags = spin_lock_irqsave(&ms_lock);
    s32 w = wheel_acc;
    wheel_acc = 0;
    spin_unlock_irqrestore(&ms_lock, flags);
    return w;
}

u64 mouse_event_count(void)
{
    return ev_count;
}

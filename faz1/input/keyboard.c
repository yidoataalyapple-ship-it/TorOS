/*
 * keyboard.c — Klavye sürücüsü (Faz 1.1)
 *
 * virtio-input EV_KEY olayları -> modifier takibi -> ASCII ring buffer.
 * API: keyboard_getchar() (blocking), keyboard_poll() (non-blocking)
 */
#include <toros/keyboard.h>
#include <toros/input.h>
#include <toros/printf.h>
#include <toros/string.h>

extern char keymap_translate(u16 code, u8 modifiers, int *is_modifier, int *mod_press);

#define KEYBUF_SIZE 256

static struct input_dev *kbd_dev;
static char keybuf[KEYBUF_SIZE];
static u32 kb_head, kb_tail;
static spinlock_t kb_lock = SPINLOCK_INIT;

static u8 modifiers;         /* bit0 shift, 1 ctrl, 2 alt, 3 caps */
static u32 ev_count;

void keyboard_init(void)
{
    kbd_dev = input_register_device("TorOS Keyboard");
    if (!kbd_dev)
        return;
    input_set_bit(kbd_dev->evbit, EV_KEY);
    kinfo("keyboard: hazır (event%u)\n", kbd_dev->id);
}

struct input_dev *keyboard_input_dev(void)
{
    return kbd_dev;
}

/* virtio_input sürücüsünden çağrılır: ham EV_KEY olayı */
void keyboard_handle_key(u16 code, s32 value)
{
    ev_count++;
    input_report_key(kbd_dev, code, value);

    int is_mod, mod_id;
    char c = keymap_translate(code, modifiers, &is_mod, &mod_id);

    if (is_mod) {
        if (mod_id == 3) {
            /* Caps Lock: sadece press'te toggle */
            if (value == 1)
                modifiers ^= BIT(3);
        } else {
            if (value == 1 || value == 2)
                modifiers |= BIT(mod_id);
            else if (value == 0)
                modifiers &= ~BIT(mod_id);
        }
        input_sync(kbd_dev);
        return;
    }

    /* value: 0=release, 1=press, 2=repeat */
    if (value == 0 || c == 0) {
        input_sync(kbd_dev);
        return;
    }

    /* Ctrl kombinasyonları: Ctrl+C = 0x03 vb. */
    if ((modifiers & BIT(1)) && c >= 'a' && c <= 'z')
        c = c - 'a' + 1;
    else if ((modifiers & BIT(1)) && c >= 'A' && c <= 'Z')
        c = c - 'A' + 1;

    u64 flags = spin_lock_irqsave(&kb_lock);
    u32 next = (kb_head + 1) % KEYBUF_SIZE;
    if (next != kb_tail) {
        keybuf[kb_head] = c;
        kb_head = next;
    }
    spin_unlock_irqrestore(&kb_lock, flags);

    input_sync(kbd_dev);
    sev();
}

char keyboard_getchar(void)
{
    char c;
    while (!keyboard_poll(&c))
        wfe();
    return c;
}

int keyboard_poll(char *out)
{
    u64 flags = spin_lock_irqsave(&kb_lock);
    if (kb_head == kb_tail) {
        spin_unlock_irqrestore(&kb_lock, flags);
        return 0;
    }
    *out = keybuf[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBUF_SIZE;
    spin_unlock_irqrestore(&kb_lock, flags);
    return 1;
}

u32 keyboard_event_count(void) { return ev_count; }
u8  keyboard_modifiers(void)   { return modifiers; }

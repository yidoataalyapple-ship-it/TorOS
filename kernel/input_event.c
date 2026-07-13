/*
 * torOS Input Event Subsystem
 * /dev/input/event* compatible event framework
 * Manages input devices, handlers, and event routing
 */

#include "../include/toros.h"
#include "../include/input.h"

/* Global input subsystem state */
static input_subsystem_t input_subsys;

/* Default event handler - routes events to global buffer */
static void default_handler_event(input_dev_t *dev, input_event_t *evt);

static input_handler_t default_handler = {
    .connect = NULL,
    .disconnect = NULL,
    .event = default_handler_event,
    .evbit = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
    .next = NULL
};

/* Default handler - writes events to global buffer */
static void default_handler_event(input_dev_t *dev, input_event_t *evt)
{
    (void)dev;
    input_event_write(&input_subsys.global_buffer, evt);
}

/* Initialize the input subsystem */
void input_subsystem_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Input Event Subsystem...\n");
    
    memset(&input_subsys, 0, sizeof(input_subsystem_t));
    spin_init(&input_subsys.subsystem_lock);
    
    /* Initialize global event buffer */
    input_event_buffer_init(&input_subsys.global_buffer);
    
    /* Register default handler */
    input_register_handler(&default_handler);
    
    input_subsys.initialized = 1;
    
    printk_color(TERM_GREEN, "[BOOT] Input subsystem ready\n");
}

/* Register an input device */
int input_register_device(input_dev_t *dev)
{
    if (!dev || !input_subsys.initialized)
        return -1;
    
    spin_lock(&input_subsys.subsystem_lock);
    
    /* Add to device list */
    dev->next = input_subsys.devices;
    input_subsys.devices = dev;
    
    printk_color(TERM_GREEN, "[INPUT] Registered device: %s\n", dev->name);
    
    /* Notify handlers */
    input_handler_t *handler = input_subsys.handlers;
    while (handler) {
        if (handler->connect) {
            handler->connect(dev);
        }
        handler = handler->next;
    }
    
    spin_unlock(&input_subsys.subsystem_lock);
    return 0;
}

/* Unregister an input device */
void input_unregister_device(input_dev_t *dev)
{
    if (!dev || !input_subsys.initialized)
        return;
    
    spin_lock(&input_subsys.subsystem_lock);
    
    /* Notify handlers */
    input_handler_t *handler = input_subsys.handlers;
    while (handler) {
        if (handler->disconnect) {
            handler->disconnect(dev);
        }
        handler = handler->next;
    }
    
    /* Remove from device list */
    input_dev_t **pp = &input_subsys.devices;
    while (*pp) {
        if (*pp == dev) {
            *pp = dev->next;
            break;
        }
        pp = &(*pp)->next;
    }
    
    printk_color(TERM_YELLOW, "[INPUT] Unregistered device: %s\n", dev->name);
    
    spin_unlock(&input_subsys.subsystem_lock);
}

/* Register an input handler */
int input_register_handler(input_handler_t *handler)
{
    if (!handler || !input_subsys.initialized)
        return -1;
    
    spin_lock(&input_subsys.subsystem_lock);
    
    handler->next = input_subsys.handlers;
    input_subsys.handlers = handler;
    
    /* Connect to existing devices */
    input_dev_t *dev = input_subsys.devices;
    while (dev) {
        if (handler->connect) {
            handler->connect(dev);
        }
        dev = dev->next;
    }
    
    spin_unlock(&input_subsys.subsystem_lock);
    return 0;
}

/* Unregister an input handler */
void input_unregister_handler(input_handler_t *handler)
{
    if (!handler || !input_subsys.initialized)
        return;
    
    spin_lock(&input_subsys.subsystem_lock);
    
    input_handler_t **pp = &input_subsys.handlers;
    while (*pp) {
        if (*pp == handler) {
            *pp = handler->next;
            break;
        }
        pp = &(*pp)->next;
    }
    
    spin_unlock(&input_subsys.subsystem_lock);
}

/* Report an input event from a device */
void input_report_event(input_dev_t *dev, uint16 type, uint16 code, int32 value)
{
    if (!dev || !input_subsys.initialized)
        return;
    
    input_event_t evt;
    evt.time_sec = rtc_get_time();
    evt.time_usec = 0;
    evt.type = type;
    evt.code = code;
    evt.value = value;
    
    /* Route to interested handlers */
    input_handler_t *handler = input_subsys.handlers;
    while (handler) {
        /* Check if handler is interested in this event type */
        if (handler->evbit[type >> 5] & (1 << (type & 0x1F))) {
            handler->event(dev, &evt);
        }
        handler = handler->next;
    }
    
    /* Call device-specific event handler */
    if (dev->event) {
        dev->event(dev, type, code, value);
    }
}

/* Synchronize device - emit SYN_REPORT */
void input_sync_device(input_dev_t *dev)
{
    input_report_event(dev, EV_SYN, SYN_REPORT, 0);
}

/* ==================== Event Buffer API ==================== */

/* Initialize an event buffer */
void input_event_buffer_init(input_event_buffer_t *buf)
{
    if (!buf) return;
    memset(buf, 0, sizeof(input_event_buffer_t));
    spin_init(&buf->lock);
    buf->head = 0;
    buf->tail = 0;
}

/* Read an event from buffer. Returns 1 if event read, 0 if empty */
int input_event_read(input_event_buffer_t *buf, input_event_t *evt)
{
    if (!buf || !evt)
        return 0;
    
    spin_lock(&buf->lock);
    
    if (buf->head == buf->tail) {
        spin_unlock(&buf->lock);
        return 0;  /* Empty */
    }
    
    *evt = buf->events[buf->tail % INPUT_EVENT_BUFFER_SIZE];
    buf->tail++;
    
    spin_unlock(&buf->lock);
    return 1;
}

/* Write an event to buffer. Returns 1 if written, 0 if full */
int input_event_write(input_event_buffer_t *buf, input_event_t *evt)
{
    if (!buf || !evt)
        return 0;
    
    spin_lock(&buf->lock);
    
    if ((buf->head - buf->tail) >= INPUT_EVENT_BUFFER_SIZE) {
        spin_unlock(&buf->lock);
        return 0;  /* Full - drop event */
    }
    
    buf->events[buf->head % INPUT_EVENT_BUFFER_SIZE] = *evt;
    buf->head++;
    
    spin_unlock(&buf->lock);
    return 1;
}

/* Check available events in buffer */
int input_event_available(input_event_buffer_t *buf)
{
    if (!buf)
        return 0;
    return buf->head - buf->tail;
}

/* ==================== Keyboard API ==================== */

/* Report a keyboard key event */
void keyboard_report_key(keyboard_device_t *kbd, uint16 code, int32 value)
{
    if (!kbd || code >= KEY_MAX)
        return;
    
    uint8 prev_state = kbd->key_state[code / 8] & (1 << (code % 8));
    
    if (value == KEY_PRESSED) {
        kbd->key_state[code / 8] |= (1 << (code % 8));
    } else if (value == KEY_RELEASED) {
        kbd->key_state[code / 8] &= ~(1 << (code % 8));
    }
    
    /* Only report if state changed */
    uint8 new_state = kbd->key_state[code / 8] & (1 << (code % 8));
    if (prev_state != new_state || value == KEY_REPEATED) {
        input_report_event(&kbd->dev, EV_KEY, code, value);
    }
    
    /* Update modifiers */
    switch (code) {
    case KEY_LEFTSHIFT:  kbd->modifiers = (kbd->modifiers & ~MOD_LSHIFT) | (value ? MOD_LSHIFT : 0); break;
    case KEY_RIGHTSHIFT: kbd->modifiers = (kbd->modifiers & ~MOD_RSHIFT) | (value ? MOD_RSHIFT : 0); break;
    case KEY_LEFTCTRL:   kbd->modifiers = (kbd->modifiers & ~MOD_LCTRL)  | (value ? MOD_LCTRL  : 0); break;
    case KEY_RIGHTCTRL:  kbd->modifiers = (kbd->modifiers & ~MOD_RCTRL)  | (value ? MOD_RCTRL  : 0); break;
    case KEY_LEFTALT:    kbd->modifiers = (kbd->modifiers & ~MOD_LALT)   | (value ? MOD_LALT   : 0); break;
    case KEY_RIGHTALT:   kbd->modifiers = (kbd->modifiers & ~MOD_RALT)   | (value ? MOD_RALT   : 0); break;
    case KEY_LEFTMETA:   kbd->modifiers = (kbd->modifiers & ~MOD_LMETA)  | (value ? MOD_LMETA  : 0); break;
    case KEY_RIGHTMETA:  kbd->modifiers = (kbd->modifiers & ~MOD_RMETA)  | (value ? MOD_RMETA  : 0); break;
    case KEY_CAPSLOCK:   if (value == KEY_PRESSED) kbd->modifiers ^= MOD_CAPS; break;
    }
}

/* Check if a key is currently pressed */
int keyboard_is_key_pressed(keyboard_device_t *kbd, uint16 code)
{
    if (!kbd || code >= KEY_MAX)
        return 0;
    return (kbd->key_state[code / 8] & (1 << (code % 8))) ? 1 : 0;
}

/* Convert scancode to ASCII character */
char keyboard_scancode_to_ascii(keyboard_device_t *kbd, uint16 code)
{
    if (!kbd)
        return 0;
    
    static const char keymap_lower[] = {
        [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
        [KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
        [KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
        [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
        [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
        [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
        [KEY_Y] = 'y', [KEY_Z] = 'z',
        [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4',
        [KEY_5] = '5', [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8',
        [KEY_9] = '9', [KEY_0] = '0',
        [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_TAB] = '\t',
        [KEY_MINUS] = '-', [KEY_EQUAL] = '=', [KEY_LEFTBRACE] = '[',
        [KEY_RIGHTBRACE] = ']', [KEY_BACKSLASH] = '\\', [KEY_SEMICOLON] = ';',
        [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`', [KEY_COMMA] = ',',
        [KEY_DOT] = '.', [KEY_SLASH] = '/',
    };
    
    static const char keymap_upper[] = {
        [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D',
        [KEY_E] = 'E', [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H',
        [KEY_I] = 'I', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
        [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O', [KEY_P] = 'P',
        [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
        [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X',
        [KEY_Y] = 'Y', [KEY_Z] = 'Z',
        [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$',
        [KEY_5] = '%', [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*',
        [KEY_9] = '(', [KEY_0] = ')',
        [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_TAB] = '\t',
        [KEY_MINUS] = '_', [KEY_EQUAL] = '+', [KEY_LEFTBRACE] = '{',
        [KEY_RIGHTBRACE] = '}', [KEY_BACKSLASH] = '|', [KEY_SEMICOLON] = ':',
        [KEY_APOSTROPHE] = '"', [KEY_GRAVE] = '~', [KEY_COMMA] = '<',
        [KEY_DOT] = '>', [KEY_SLASH] = '?',
    };
    
    int shift = (kbd->modifiers & (MOD_LSHIFT | MOD_RSHIFT)) ? 1 : 0;
    int caps = (kbd->modifiers & MOD_CAPS) ? 1 : 0;
    int alpha = (code >= KEY_A && code <= KEY_Z);
    
    if (shift ^ caps && alpha) {
        if (code < sizeof(keymap_upper))
            return keymap_upper[code];
    } else {
        if (code < sizeof(keymap_lower))
            return keymap_lower[code];
    }
    
    return 0;
}

/* ==================== Mouse API ==================== */

/* Report mouse movement */
void mouse_report_movement(mouse_device_t *mouse, int32 dx, int32 dy)
{
    if (!mouse)
        return;
    
    /* Apply acceleration */
    if (mouse->accel_numerator > 0 && mouse->accel_denominator > 0) {
        if ((dx > (int32)mouse->threshold) || (dx < -(int32)mouse->threshold))
            dx = dx * mouse->accel_numerator / mouse->accel_denominator;
        if ((dy > (int32)mouse->threshold) || (dy < -(int32)mouse->threshold))
            dy = dy * mouse->accel_numerator / mouse->accel_denominator;
    }
    
    mouse->x += dx;
    mouse->y += dy;
    
    /* Clamp to screen bounds */
    if (mouse->x < 0) mouse->x = 0;
    if (mouse->y < 0) mouse->y = 0;
    if ((uint32)mouse->x >= mouse->screen_width) mouse->x = mouse->screen_width - 1;
    if ((uint32)mouse->y >= mouse->screen_height) mouse->y = mouse->screen_height - 1;
    
    input_report_event(&mouse->dev, EV_REL, REL_X, dx);
    input_report_event(&mouse->dev, EV_REL, REL_Y, dy);
}

/* Report mouse button */
void mouse_report_button(mouse_device_t *mouse, uint32 button, int32 pressed)
{
    if (!mouse || button < BTN_MOUSE || button > BTN_BACK)
        return;
    
    uint32 mask = 1 << (button - BTN_MOUSE);
    
    if (pressed) {
        mouse->button_state |= mask;
    } else {
        mouse->button_state &= ~mask;
    }
    
    input_report_event(&mouse->dev, EV_KEY, button, pressed ? KEY_PRESSED : KEY_RELEASED);
}

/* Report mouse wheel */
void mouse_report_wheel(mouse_device_t *mouse, int32 delta)
{
    if (!mouse)
        return;
    
    mouse->wheel += delta;
    input_report_event(&mouse->dev, EV_REL, REL_WHEEL, delta);
}

/* Set mouse screen bounds */
void mouse_set_bounds(mouse_device_t *mouse, uint32 width, uint32 height)
{
    if (!mouse)
        return;
    
    mouse->screen_width = width;
    mouse->screen_height = height;
    
    /* Clamp current position */
    if ((uint32)mouse->x >= width) mouse->x = width - 1;
    if ((uint32)mouse->y >= height) mouse->y = height - 1;
}

/* Poll all input devices */
void input_poll_all(void)
{
    /* Poll VirtIO input */
    extern void virtio_input_poll(void);
    virtio_input_poll();
    
    /* Poll USB HID */
    extern void usb_hid_poll(void);
    usb_hid_poll();
}

/* Get global event count */
int input_get_global_event_count(void)
{
    return input_event_available(&input_subsys.global_buffer);
}

/* Read from global event buffer */
int input_read_global_event(input_event_t *evt)
{
    return input_event_read(&input_subsys.global_buffer, evt);
}

/* Dump input device list */
void input_dump_devices(void)
{
    printk_color(TERM_CYAN, "\n=== Input Devices ===\n");
    
    spin_lock(&input_subsys.subsystem_lock);
    
    input_dev_t *dev = input_subsys.devices;
    int count = 0;
    
    while (dev) {
        printk_color(TERM_GREEN, "  %s (Vendor:%04X Product:%04X)\n",
                     dev->name, dev->id_vendor, dev->id_product);
        count++;
        dev = dev->next;
    }
    
    if (count == 0) {
        printk_color(TERM_YELLOW, "  (no devices)\n");
    }
    
    printk_color(TERM_CYAN, "  Total: %d devices\n", count);
    printk_color(TERM_CYAN, "  Global buffer: %d events\n\n",
                 input_event_available(&input_subsys.global_buffer));
    
    spin_unlock(&input_subsys.subsystem_lock);
}

/* Wait for an input event (blocking) */
int input_wait_for_event(input_event_t *evt, uint32 timeout_ms)
{
    if (!evt)
        return 0;
    
    uint64 start = get_jiffies();
    
    while (1) {
        if (input_read_global_event(evt))
            return 1;
        
        if (timeout_ms > 0 && (get_jiffies() - start) > timeout_ms)
            return 0;  /* Timeout */
        
        /* Poll hardware */
        input_poll_all();
        
        /* Small delay to prevent busy-waiting */
        wfe();
    }
}

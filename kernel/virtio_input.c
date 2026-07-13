/*
 * torOS VirtIO Input Driver
 * PCI-based virtio-input for QEMU virt machine
 * Supports keyboard and mouse input via VirtIO
 */

#include "../include/toros.h"
#include "../include/virtio.h"
#include "../include/input.h"

/* QEMU virt VirtIO input PCI base */
#define VIRTIO_INPUT_PCI_BASE   0x09001000

/* VirtIO input PCI BAR0 */
static volatile uint32 *virtio_input_regs = (volatile uint32 *)VIRTIO_INPUT_PCI_BASE;

/* Static device structures */
static virtio_device_t virtio_input_dev;
static virtqueue_t event_queue;
static virtqueue_t status_queue;

/* Input event buffers */
static virtio_input_event_t event_buffers[16];
static input_dev_t virtio_input_device;
static keyboard_device_t virtio_kbd;
static mouse_device_t virtio_mouse;

/* HID scancode to keycode mapping (USB HID -> torOS keycodes) */
static const uint8 hid_to_keycode[] = {
    [0x04] = KEY_A,         /* a */
    [0x05] = KEY_B,
    [0x06] = KEY_C,
    [0x07] = KEY_D,
    [0x08] = KEY_E,
    [0x09] = KEY_F,
    [0x0A] = KEY_G,
    [0x0B] = KEY_H,
    [0x0C] = KEY_I,
    [0x0D] = KEY_J,
    [0x0E] = KEY_K,
    [0x0F] = KEY_L,
    [0x10] = KEY_M,
    [0x11] = KEY_N,
    [0x12] = KEY_O,
    [0x13] = KEY_P,
    [0x14] = KEY_Q,
    [0x15] = KEY_R,
    [0x16] = KEY_S,
    [0x17] = KEY_T,
    [0x18] = KEY_U,
    [0x19] = KEY_V,
    [0x1A] = KEY_W,
    [0x1B] = KEY_X,
    [0x1C] = KEY_Y,
    [0x1D] = KEY_Z,
    [0x1E] = KEY_1,
    [0x1F] = KEY_2,
    [0x20] = KEY_3,
    [0x21] = KEY_4,
    [0x22] = KEY_5,
    [0x23] = KEY_6,
    [0x24] = KEY_7,
    [0x25] = KEY_8,
    [0x26] = KEY_9,
    [0x27] = KEY_0,
    [0x28] = KEY_ENTER,
    [0x29] = KEY_ESC,
    [0x2A] = KEY_BACKSPACE,
    [0x2B] = KEY_TAB,
    [0x2C] = KEY_SPACE,
    [0x2D] = KEY_MINUS,
    [0x2E] = KEY_EQUAL,
    [0x2F] = KEY_LEFTBRACE,
    [0x30] = KEY_RIGHTBRACE,
    [0x31] = KEY_BACKSLASH,
    [0x33] = KEY_SEMICOLON,
    [0x34] = KEY_APOSTROPHE,
    [0x35] = KEY_GRAVE,
    [0x36] = KEY_COMMA,
    [0x37] = KEY_DOT,
    [0x38] = KEY_SLASH,
    [0x39] = KEY_CAPSLOCK,
    [0x3A] = KEY_F1,
    [0x3B] = KEY_F2,
    [0x3C] = KEY_F3,
    [0x3D] = KEY_F4,
    [0x3E] = KEY_F5,
    [0x3F] = KEY_F6,
    [0x40] = KEY_F7,
    [0x41] = KEY_F8,
    [0x42] = KEY_F9,
    [0x43] = KEY_F10,
    [0x44] = KEY_F11,
    [0x45] = KEY_F12,
    [0x46] = KEY_SYSRQ,     /* Print Screen */
    [0x47] = KEY_SCROLLLOCK,
    [0x48] = KEY_PAUSE,
    [0x49] = KEY_INSERT,
    [0x4A] = KEY_HOME,
    [0x4B] = KEY_PAGEUP,
    [0x4C] = KEY_DELETE,
    [0x4D] = KEY_END,
    [0x4E] = KEY_PAGEDOWN,
    [0x4F] = KEY_RIGHT,
    [0x50] = KEY_LEFT,
    [0x51] = KEY_DOWN,
    [0x52] = KEY_UP,
    [0x53] = KEY_NUMLOCK,
    [0x54] = KEY_KPSLASH,
    [0x55] = KEY_KPASTERISK,
    [0x56] = KEY_KPMINUS,
    [0x57] = KEY_KPPLUS,
    [0x58] = KEY_KPENTER,
    [0x59] = KEY_KP1,
    [0x5A] = KEY_KP2,
    [0x5B] = KEY_KP3,
    [0x5C] = KEY_KP4,
    [0x5D] = KEY_KP5,
    [0x5E] = KEY_KP6,
    [0x5F] = KEY_KP7,
    [0x60] = KEY_KP8,
    [0x61] = KEY_KP9,
    [0x62] = KEY_KP0,
    [0x63] = KEY_KPDOT,
    [0x64] = KEY_102ND,
    [0xE0] = KEY_LEFTCTRL,
    [0xE1] = KEY_LEFTSHIFT,
    [0xE2] = KEY_LEFTALT,
    [0xE3] = KEY_LEFTMETA,
    [0xE4] = KEY_RIGHTCTRL,
    [0xE5] = KEY_RIGHTSHIFT,
    [0xE6] = KEY_RIGHTALT,
    [0xE7] = KEY_RIGHTMETA,
};

/* Modifier mapping */
#define HID_LCTRL   0xE0
#define HID_LSHIFT  0xE1
#define HID_LALT    0xE2
#define HID_LMETA   0xE3
#define HID_RCTRL   0xE4
#define HID_RSHIFT  0xE5
#define HID_RALT    0xE6
#define HID_RMETA   0xE7

/* Read from PCI BAR */
static inline uint32 virtio_read(uint32 offset)
{
    return virtio_input_regs[offset >> 2];
}

static inline void virtio_write(uint32 offset, uint32 val)
{
    virtio_input_regs[offset >> 2] = val;
}

/* Parse HID report and generate input events */
static void parse_hid_keyboard_report(uint8 *report, int len)
{
    if (len < 8) return;
    
    uint8 modifiers = report[0];
    uint8 keycodes[6] = {report[2], report[3], report[4], report[5], report[6], report[7]};
    
    /* Report modifier keys */
    if (modifiers & 0x01) keyboard_report_key(&virtio_kbd, KEY_LEFTCTRL, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_LEFTCTRL, KEY_RELEASED);
    
    if (modifiers & 0x02) keyboard_report_key(&virtio_kbd, KEY_LEFTSHIFT, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_LEFTSHIFT, KEY_RELEASED);
    
    if (modifiers & 0x04) keyboard_report_key(&virtio_kbd, KEY_LEFTALT, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_LEFTALT, KEY_RELEASED);
    
    if (modifiers & 0x08) keyboard_report_key(&virtio_kbd, KEY_LEFTMETA, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_LEFTMETA, KEY_RELEASED);
    
    if (modifiers & 0x10) keyboard_report_key(&virtio_kbd, KEY_RIGHTCTRL, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_RIGHTCTRL, KEY_RELEASED);
    
    if (modifiers & 0x20) keyboard_report_key(&virtio_kbd, KEY_RIGHTSHIFT, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_RIGHTSHIFT, KEY_RELEASED);
    
    if (modifiers & 0x40) keyboard_report_key(&virtio_kbd, KEY_RIGHTALT, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_RIGHTALT, KEY_RELEASED);
    
    if (modifiers & 0x80) keyboard_report_key(&virtio_kbd, KEY_RIGHTMETA, KEY_PRESSED);
    else keyboard_report_key(&virtio_kbd, KEY_RIGHTMETA, KEY_RELEASED);
    
    /* Report regular keys */
    for (int i = 0; i < 6; i++) {
        if (keycodes[i] > 0 && keycodes[i] < sizeof(hid_to_keycode)) {
            uint16 keycode = hid_to_keycode[keycodes[i]];
            if (keycode != 0) {
                keyboard_report_key(&virtio_kbd, keycode, KEY_PRESSED);
            }
        }
    }
    
    input_sync_device(&virtio_kbd.dev);
}

/* Parse HID mouse report */
static void parse_hid_mouse_report(uint8 *report, int len)
{
    if (len < 3) return;
    
    uint8 buttons = report[0];
    int8 dx = (int8)report[1];
    int8 dy = (int8)report[2];
    int8 wheel = (len >= 4) ? (int8)report[3] : 0;
    
    /* Report buttons */
    if (buttons & 0x01) mouse_report_button(&virtio_mouse, BTN_LEFT, 1);
    else mouse_report_button(&virtio_mouse, BTN_LEFT, 0);
    
    if (buttons & 0x02) mouse_report_button(&virtio_mouse, BTN_RIGHT, 1);
    else mouse_report_button(&virtio_mouse, BTN_RIGHT, 0);
    
    if (buttons & 0x04) mouse_report_button(&virtio_mouse, BTN_MIDDLE, 1);
    else mouse_report_button(&virtio_mouse, BTN_MIDDLE, 0);
    
    /* Report movement */
    if (dx != 0 || dy != 0) {
        mouse_report_movement(&virtio_mouse, dx, dy);
    }
    
    /* Report wheel */
    if (wheel != 0) {
        mouse_report_wheel(&virtio_mouse, wheel);
    }
    
    input_sync_device(&virtio_mouse.dev);
}

/* Process received VirtIO input event */
static void process_virtio_event(virtio_input_event_t *evt)
{
    switch (evt->type) {
    case EV_KEY:
        /* Keyboard event */
        if (evt->code < KEY_MAX) {
            keyboard_report_key(&virtio_kbd, evt->code, evt->value);
        }
        /* Mouse button */
        else if (evt->code >= BTN_MOUSE && evt->code <= BTN_BACK) {
            mouse_report_button(&virtio_mouse, evt->code, evt->value);
        }
        break;
        
    case EV_REL:
        if (evt->code == REL_X) {
            mouse_report_movement(&virtio_mouse, evt->value, 0);
        } else if (evt->code == REL_Y) {
            mouse_report_movement(&virtio_mouse, 0, evt->value);
        } else if (evt->code == REL_WHEEL) {
            mouse_report_wheel(&virtio_mouse, evt->value);
        }
        break;
        
    case EV_SYN:
        if (evt->code == SYN_REPORT) {
            input_sync_device(&virtio_kbd.dev);
            input_sync_device(&virtio_mouse.dev);
        }
        break;
    }
}

/* Poll VirtIO input device for events */
void virtio_input_poll(void)
{
    if (!virtio_input_dev.status & VIRTIO_STATUS_DRIVER_OK)
        return;
    
    /* Check for used buffers */
    while (event_queue.last_used_idx != event_queue.used->idx) {
        vring_used_elem_t *used_elem = &event_queue.used->ring[event_queue.last_used_idx % event_queue.queue_size];
        uint32 desc_id = used_elem->id;
        uint32 len = used_elem->len;
        
        if (desc_id < 16 && len >= sizeof(virtio_input_event_t)) {
            virtio_input_event_t *evt = &event_buffers[desc_id];
            process_virtio_event(evt);
        }
        
        /* Return descriptor to available ring */
        event_queue.avail->ring[event_queue.avail->idx % event_queue.queue_size] = desc_id;
        __sync_synchronize();
        event_queue.avail->idx++;
        
        virtio_write(VIRTIO_PCI_QUEUE_NOTIFY, 0);
        
        event_queue.last_used_idx++;
    }
}

/* VirtIO input interrupt handler */
void virtio_input_handle_interrupt(virtio_device_t *dev)
{
    (void)dev;
    virtio_input_poll();
}

/* Initialize VirtIO input device */
void virtio_input_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] VirtIO Input...\n");
    
    /* Initialize PCI device */
    memset(&virtio_input_dev, 0, sizeof(virtio_device_t));
    virtio_input_dev.device_id = VIRTIO_ID_INPUT;
    virtio_input_dev.vendor_id = VIRTIO_PCI_VENDOR_ID;
    virtio_input_dev.io_base = VIRTIO_INPUT_PCI_BASE;
    
    /* Reset device */
    virtio_write(VIRTIO_PCI_STATUS, 0);
    
    /* Acknowledge */
    virtio_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    
    /* Driver present */
    virtio_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    
    /* Negotiate features */
    uint32 features = virtio_read(VIRTIO_PCI_HOST_FEATURES);
    features &= ~(1 << VIRTIO_F_VERSION_1);  /* Use legacy mode */
    virtio_write(VIRTIO_PCI_GUEST_FEATURES, features);
    
    /* Features OK */
    virtio_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    
    /* Setup event queue (queue 0) */
    virtio_write(VIRTIO_PCI_QUEUE_SEL, 0);
    uint32 queue_size = virtio_read(VIRTIO_PCI_QUEUE_NUM);
    if (queue_size > 16) queue_size = 16;
    
    event_queue.queue_size = queue_size;
    event_queue.queue_index = 0;
    event_queue.last_used_idx = 0;
    
    /* Allocate queue pages */
    uint32 queue_pages_size = (sizeof(vring_desc_t) * queue_size + sizeof(vring_avail_t) + sizeof(uint16) * queue_size + sizeof(vring_used_t) + sizeof(vring_used_elem_t) * queue_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    event_queue.queue_pages = page_alloc();
    if (!event_queue.queue_pages) {
        printk_color(TERM_RED, "[VIRTIO-INPUT] Failed to allocate queue\n");
        return;
    }
    memset(event_queue.queue_pages, 0, PAGE_SIZE);
    
    /* Setup ring pointers */
    event_queue.desc = (vring_desc_t *)event_queue.queue_pages;
    event_queue.avail = (vring_avail_t *)((uint8 *)event_queue.queue_pages + sizeof(vring_desc_t) * queue_size);
    event_queue.used = (vring_used_t *)((uint8 *)event_queue.queue_pages + PAGE_SIZE / 2);
    
    /* Setup descriptors */
    for (uint32 i = 0; i < queue_size; i++) {
        event_queue.desc[i].addr = (uint64)&event_buffers[i];
        event_queue.desc[i].len = sizeof(virtio_input_event_t);
        event_queue.desc[i].flags = VRING_DESC_F_WRITE;
        event_queue.desc[i].next = (i + 1 < queue_size) ? i + 1 : 0;
        
        /* Add to available ring */
        event_queue.avail->ring[i] = i;
    }
    event_queue.avail->idx = queue_size;
    
    /* Set PFN */
    uint64 queue_pfn = (uint64)event_queue.queue_pages;
    virtio_write(VIRTIO_PCI_QUEUE_PFN, (uint32)(queue_pfn >> 12));
    
    /* DRIVER_OK */
    virtio_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    
    /* Notify queue */
    virtio_write(VIRTIO_PCI_QUEUE_NOTIFY, 0);
    
    /* Initialize keyboard device */
    memset(&virtio_kbd, 0, sizeof(keyboard_device_t));
    strcpy(virtio_kbd.dev.name, "VirtIO Keyboard");
    virtio_kbd.dev.id_vendor = VIRTIO_PCI_VENDOR_ID;
    virtio_kbd.dev.id_product = VIRTIO_ID_INPUT;
    virtio_kbd.dev.evbit[0] = (1 << EV_KEY) | (1 << EV_SYN);
    virtio_kbd.dev.keybit[0] = 0xFFFFFFFF;
    virtio_kbd.dev.keybit[1] = 0xFFFFFFFF;
    virtio_kbd.dev.keybit[2] = 0xFFFFFFFF;
    virtio_kbd.dev.keybit[3] = 0xFFFFFFFF;
    
    /* Initialize mouse device */
    memset(&virtio_mouse, 0, sizeof(mouse_device_t));
    strcpy(virtio_mouse.dev.name, "VirtIO Mouse");
    virtio_mouse.dev.id_vendor = VIRTIO_PCI_VENDOR_ID;
    virtio_mouse.dev.id_product = VIRTIO_ID_INPUT;
    virtio_mouse.dev.evbit[0] = (1 << EV_KEY) | (1 << EV_REL) | (1 << EV_SYN);
    virtio_mouse.dev.keybit[(BTN_MOUSE >> 5)] = (1 << (BTN_MOUSE & 0x1F)) | (1 << (BTN_LEFT & 0x1F)) | (1 << (BTN_RIGHT & 0x1F)) | (1 << (BTN_MIDDLE & 0x1F));
    virtio_mouse.dev.relbit[0] = (1 << REL_X) | (1 << REL_Y) | (1 << REL_WHEEL);
    virtio_mouse.screen_width = FB_WIDTH;
    virtio_mouse.screen_height = FB_HEIGHT;
    
    /* Register devices */
    input_register_device(&virtio_kbd.dev);
    input_register_device(&virtio_mouse.dev);
    
    printk_color(TERM_GREEN, "[BOOT] VirtIO Input: Keyboard + Mouse ready\n");
    printk_color(TERM_GREEN, "[BOOT]   Queue size: %d, Buffers: %d\n", queue_size, queue_size);
}

/* Check if VirtIO input has pending events */
int virtio_input_has_event(void)
{
    return event_queue.last_used_idx != event_queue.used->idx;
}

/* Get next VirtIO input event */
virtio_input_event_t virtio_input_get_event(void)
{
    virtio_input_event_t evt = {0, 0, 0};
    
    if (virtio_input_has_event()) {
        vring_used_elem_t *used_elem = &event_queue.used->ring[event_queue.last_used_idx % event_queue.queue_size];
        uint32 desc_id = used_elem->id;
        
        if (desc_id < 16) {
            evt = event_buffers[desc_id];
        }
        
        /* Return buffer */
        event_queue.avail->ring[event_queue.avail->idx % event_queue.queue_size] = desc_id;
        __sync_synchronize();
        event_queue.avail->idx++;
        virtio_write(VIRTIO_PCI_QUEUE_NOTIFY, 0);
        
        event_queue.last_used_idx++;
    }
    
    return evt;
}

/* Shutdown VirtIO input */
void virtio_input_shutdown(void)
{
    virtio_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
    printk_color(TERM_YELLOW, "[VIRTIO-INPUT] Shutdown\n");
}

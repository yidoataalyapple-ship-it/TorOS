/*
 * torOS USB xHCI Host Controller Driver
 * QEMU xHCI controller support
 * USB 2.0/3.0 hub enumeration + HID keyboard/mouse
 */

#include "../include/toros.h"
#include "../include/usb.h"
#include "../include/input.h"

/* QEMU virt xHCI base address */

/* xHCI runtime structure */
static xhci_controller_t xhci;

/* Static input devices for USB HID */
static keyboard_device_t usb_kbd;
static mouse_device_t usb_mouse;
static int usb_hid_initialized = 0;

/* Read xHCI capability register */
static inline uint32 xhci_cap_read(uint32 offset)
{
    return *(volatile uint32 *)(xhci.mmio_base + offset);
}

/* Read xHCI operational register */
static inline uint32 xhci_op_read(uint32 offset)
{
    return *(volatile uint32 *)(xhci.mmio_base + xhci.rt_offset + offset);
}

/* Write xHCI operational register */
static inline void xhci_op_write(uint32 offset, uint32 val)
{
    *(volatile uint32 *)(xhci.mmio_base + xhci.rt_offset + offset) = val;
}

/* Read xHCI port status */
static inline uint32 xhci_port_read(uint32 port, uint32 offset)
{
    return *(volatile uint32 *)(xhci.mmio_base + xhci.rt_offset + XHCI_PORTSC(port) + offset);
}

/* Write xHCI port status */
static inline void xhci_port_write(uint32 port, uint32 offset, uint32 val)
{
    *(volatile uint32 *)(xhci.mmio_base + xhci.rt_offset + XHCI_PORTSC(port) + offset) = val;
}

/* USB HID boot protocol keyboard scancode to keycode */
uint16 usb_scancode_to_keycode(uint8 scancode)
{
    static const uint16 scancode_map[] = {
        [0x04] = KEY_A, [0x05] = KEY_B, [0x06] = KEY_C, [0x07] = KEY_D,
        [0x08] = KEY_E, [0x09] = KEY_F, [0x0A] = KEY_G, [0x0B] = KEY_H,
        [0x0C] = KEY_I, [0x0D] = KEY_J, [0x0E] = KEY_K, [0x0F] = KEY_L,
        [0x10] = KEY_M, [0x11] = KEY_N, [0x12] = KEY_O, [0x13] = KEY_P,
        [0x14] = KEY_Q, [0x15] = KEY_R, [0x16] = KEY_S, [0x17] = KEY_T,
        [0x18] = KEY_U, [0x19] = KEY_V, [0x1A] = KEY_W, [0x1B] = KEY_X,
        [0x1C] = KEY_Y, [0x1D] = KEY_Z,
        [0x1E] = KEY_1, [0x1F] = KEY_2, [0x20] = KEY_3, [0x21] = KEY_4,
        [0x22] = KEY_5, [0x23] = KEY_6, [0x24] = KEY_7, [0x25] = KEY_8,
        [0x26] = KEY_9, [0x27] = KEY_0,
        [0x28] = KEY_ENTER, [0x29] = KEY_ESC, [0x2A] = KEY_BACKSPACE,
        [0x2B] = KEY_TAB, [0x2C] = KEY_SPACE, [0x2D] = KEY_MINUS,
        [0x2E] = KEY_EQUAL, [0x2F] = KEY_LEFTBRACE, [0x30] = KEY_RIGHTBRACE,
        [0x31] = KEY_BACKSLASH, [0x33] = KEY_SEMICOLON, [0x34] = KEY_APOSTROPHE,
        [0x35] = KEY_GRAVE, [0x36] = KEY_COMMA, [0x37] = KEY_DOT,
        [0x38] = KEY_SLASH, [0x39] = KEY_CAPSLOCK,
        [0x3A] = KEY_F1, [0x3B] = KEY_F2, [0x3C] = KEY_F3, [0x3D] = KEY_F4,
        [0x3E] = KEY_F5, [0x3F] = KEY_F6, [0x40] = KEY_F7, [0x41] = KEY_F8,
        [0x42] = KEY_F9, [0x43] = KEY_F10, [0x44] = KEY_F11, [0x45] = KEY_F12,
        [0x46] = KEY_SYSRQ, [0x47] = KEY_SCROLLLOCK, [0x48] = KEY_PAUSE,
        [0x49] = KEY_INSERT, [0x4A] = KEY_HOME, [0x4B] = KEY_PAGEUP,
        [0x4C] = KEY_DELETE, [0x4D] = KEY_END, [0x4E] = KEY_PAGEDOWN,
        [0x4F] = KEY_RIGHT, [0x50] = KEY_LEFT, [0x51] = KEY_DOWN, [0x52] = KEY_UP,
    };
    
    if (scancode < sizeof(scancode_map) / sizeof(scancode_map[0]))
        return scancode_map[scancode];
    return 0;
}

/* Initialize xHCI controller */
int xhci_init(uint64 mmio_base)
{
    printk_color(TERM_YELLOW, "[BOOT] USB xHCI...\n");
    
    memset(&xhci, 0, sizeof(xhci_controller_t));
    xhci.mmio_base = mmio_base;
    
    /* Read capabilities */
    uint8 caplength = xhci_cap_read(XHCI_CAPLENGTH) & 0xFF;
    uint16 hciversion = (xhci_cap_read(XHCI_CAPLENGTH) >> 16) & 0xFFFF;
    uint32 hcsparams1 = xhci_cap_read(XHCI_HCSPARAMS1);
    uint32 hccparams1 = xhci_cap_read(XHCI_HCCPARAMS1);
    
    xhci.max_slots = hcsparams1 & 0xFF;
    xhci.max_ports = (hcsparams1 >> 24) & 0xFF;
    xhci.rt_offset = caplength;
    xhci.db_offset = xhci_cap_read(XHCI_DBOFF);
    
    printk_color(TERM_CYAN, "[USB] xHCI v%d.%d, Slots: %d, Ports: %d\n",
                 (hciversion >> 8) & 0xFF, hciversion & 0xFF,
                 xhci.max_slots, xhci.max_ports);
    
    /* Reset controller */
    if (xhci_reset() < 0) {
        printk_color(TERM_RED, "[USB] xHCI reset failed\n");
        return -1;
    }
    
    /* Start controller */
    if (xhci_start() < 0) {
        printk_color(TERM_RED, "[USB] xHCI start failed\n");
        return -1;
    }
    
    xhci.initialized = 1;
    
    printk_color(TERM_GREEN, "[BOOT] USB xHCI ready (%d ports)\n", xhci.max_ports);
    return 0;
}

/* Reset xHCI controller */
int xhci_reset(void)
{
    /* Halt controller first */
    xhci_op_write(XHCI_USBCMD, 0);
    
    /* Wait for halt */
    int timeout = 1000;
    while (!(xhci_op_read(XHCI_USBSTS) & XHCI_STS_HCH)) {
        if (--timeout <= 0) break;
        rtc_mdelay(1);
    }
    
    /* Reset */
    xhci_op_write(XHCI_USBCMD, XHCI_CMD_HCRESET);
    
    timeout = 1000;
    while (xhci_op_read(XHCI_USBCMD) & XHCI_CMD_HCRESET) {
        if (--timeout <= 0) {
            printk_color(TERM_RED, "[USB] xHCI reset timeout\n");
            return -1;
        }
        rtc_mdelay(1);
    }
    
    /* Wait for CNR clear */
    timeout = 1000;
    while (xhci_op_read(XHCI_USBSTS) & XHCI_STS_CNR) {
        if (--timeout <= 0) break;
        rtc_mdelay(1);
    }
    
    return 0;
}

/* Start xHCI controller */
int xhci_start(void)
{
    /* Set max slots */
    xhci_op_write(XHCI_CONFIG, xhci.max_slots);
    
    /* Allocate and set DCBAA */
    xhci.dcbaa = (uint32 *)page_alloc();
    if (!xhci.dcbaa) {
        printk_color(TERM_RED, "[USB] Failed to allocate DCBAA\n");
        return -1;
    }
    memset(xhci.dcbaa, 0, PAGE_SIZE);
    xhci_op_write(XHCI_DCBAAP, (uint32)(uint64)xhci.dcbaa);
    xhci_op_write(XHCI_DCBAAP + 4, (uint32)((uint64)xhci.dcbaa >> 32));
    
    /* Set CRCR */
    xhci.crcr = (uint64 *)page_alloc();
    if (!xhci.crcr) {
        printk_color(TERM_RED, "[USB] Failed to allocate CRCR\n");
        return -1;
    }
    memset(xhci.crcr, 0, PAGE_SIZE);
    xhci_op_write(XHCI_CRCR, (uint32)(uint64)xhci.crcr | 1);
    xhci_op_write(XHCI_CRCR + 4, (uint32)((uint64)xhci.crcr >> 32));
    
    /* Set page size */
    xhci.page_size = xhci_op_read(XHCI_PAGESIZE) << 12;
    
    /* Enable interrupts and start */
    xhci_op_write(XHCI_USBCMD, XHCI_CMD_RUN | XHCI_CMD_INTE | XHCI_CMD_HSEE);
    
    /* Wait for running */
    int timeout = 1000;
    while (xhci_op_read(XHCI_USBSTS) & XHCI_STS_HCH) {
        if (--timeout <= 0) {
            printk_color(TERM_RED, "[USB] xHCI start timeout\n");
            return -1;
        }
        rtc_mdelay(1);
    }
    
    return 0;
}

/* Stop xHCI controller */
void xhci_stop(void)
{
    xhci_op_write(XHCI_USBCMD, 0);
    int timeout = 1000;
    while (!(xhci_op_read(XHCI_USBSTS) & XHCI_STS_HCH)) {
        if (--timeout <= 0) break;
        rtc_mdelay(1);
    }
}

/* Reset a port */
int xhci_port_reset(uint32 port)
{
    if (port >= xhci.max_ports)
        return -1;
    
    uint32 portsc = xhci_port_read(port, 0);
    
    /* Check if device connected */
    if (!(portsc & XHCI_PORT_CCS)) {
        return -1;  /* No device */
    }
    
    printk_color(TERM_CYAN, "[USB] Port %d: Device connected, resetting...\n", port);
    
    /* Reset port */
    portsc = (portsc & ~XHCI_PORT_PR) | XHCI_PORT_PR;
    xhci_port_write(port, 0, portsc);
    
    /* Wait for reset complete */
    int timeout = 1000;
    while (1) {
        portsc = xhci_port_read(port, 0);
        if (!(portsc & XHCI_PORT_PR))
            break;
        if (--timeout <= 0) {
            printk_color(TERM_RED, "[USB] Port %d reset timeout\n", port);
            return -1;
        }
        rtc_mdelay(1);
    }
    
    /* Check if enabled */
    if (!(portsc & XHCI_PORT_PED)) {
        printk_color(TERM_RED, "[USB] Port %d not enabled\n", port);
        return -1;
    }
    
    printk_color(TERM_GREEN, "[USB] Port %d: Ready (speed=%d)\n", port,
                 (portsc >> 10) & 0xF);
    return 0;
}

/* Handle port connection events */
void xhci_handle_port_change(void)
{
    if (!xhci.initialized)
        return;
    
    for (uint32 port = 1; port <= xhci.max_ports; port++) {
        uint32 portsc = xhci_port_read(port - 1, 0);
        
        /* Connection status change */
        if (portsc & XHCI_PORT_CSC) {
            /* Clear status change */
            xhci_port_write(port - 1, 0, portsc | XHCI_PORT_CSC);
            
            if (portsc & XHCI_PORT_CCS) {
                /* Device connected */
                printk_color(TERM_GREEN, "[USB] Port %d: Device connected\n", port);
                usb_hotplug_handle_connect(port);
            } else {
                /* Device disconnected */
                printk_color(TERM_YELLOW, "[USB] Port %d: Device disconnected\n", port);
                usb_hotplug_handle_disconnect(port);
            }
        }
        
        /* Reset complete */
        if (portsc & XHCI_PORT_PRC) {
            xhci_port_write(port - 1, 0, portsc | XHCI_PORT_PRC);
        }
    }
}

/* Handle xHCI events */
void xhci_handle_events(void)
{
    if (!xhci.initialized)
        return;
    
    uint32 usbsts = xhci_op_read(XHCI_USBSTS);
    
    /* Port change detect */
    if (usbsts & XHCI_STS_PCD) {
        xhci_op_write(XHCI_USBSTS, XHCI_STS_PCD);
        xhci_handle_port_change();
    }
    
    /* Event interrupt */
    if (usbsts & XHCI_STS_EINT) {
        xhci_op_write(XHCI_USBSTS, XHCI_STS_EINT);
    }
}

/* ==================== USB Hot-plug ==================== */

void usb_hotplug_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] USB Hot-plug...\n");
    
    /* Initialize USB HID input devices */
    memset(&usb_kbd, 0, sizeof(keyboard_device_t));
    strcpy(usb_kbd.dev.name, "USB Keyboard");
    usb_kbd.dev.id_vendor = 0x0000;
    usb_kbd.dev.id_product = 0x0000;
    usb_kbd.dev.evbit[0] = (1 << EV_KEY) | (1 << EV_SYN);
    usb_kbd.dev.keybit[0] = 0xFFFFFFFF;
    
    memset(&usb_mouse, 0, sizeof(mouse_device_t));
    strcpy(usb_mouse.dev.name, "USB Mouse");
    usb_mouse.dev.id_vendor = 0x0000;
    usb_mouse.dev.id_product = 0x0000;
    usb_mouse.dev.evbit[0] = (1 << EV_KEY) | (1 << EV_REL) | (1 << EV_SYN);
    usb_mouse.dev.relbit[0] = (1 << REL_X) | (1 << REL_Y) | (1 << REL_WHEEL);
    usb_mouse.screen_width = FB_WIDTH;
    usb_mouse.screen_height = FB_HEIGHT;
    
    usb_hid_initialized = 1;
    
    printk_color(TERM_GREEN, "[BOOT] USB Hot-plug ready\n");
}

void usb_hotplug_poll(void)
{
    if (!xhci.initialized)
        return;
    
    xhci_handle_events();
}

void usb_hotplug_handle_connect(uint32 port)
{
    printk_color(TERM_GREEN, "[USB] Hot-plug: New device on port %d\n", port);
    
    /* Reset port */
    if (xhci_port_reset(port - 1) < 0)
        return;
    
    /* TODO: Full USB enumeration */
    /* For now, register generic HID devices */
    if (usb_hid_initialized) {
        input_register_device(&usb_kbd.dev);
        input_register_device(&usb_mouse.dev);
    }
}

void usb_hotplug_handle_disconnect(uint32 port)
{
    printk_color(TERM_YELLOW, "[USB] Hot-plug: Device removed from port %d\n", port);
    
    /* Unregister HID devices */
    if (usb_hid_initialized) {
        input_unregister_device(&usb_kbd.dev);
        input_unregister_device(&usb_mouse.dev);
    }
}

/* ==================== USB HID ==================== */

void usb_hid_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] USB HID...\n");
    printk_color(TERM_GREEN, "[BOOT] USB HID ready\n");
}

int usb_hid_probe(usb_device_t *dev)
{
    if (!dev || dev->class != USB_CLASS_HID)
        return -1;
    
    printk_color(TERM_GREEN, "[USB-HID] Probing interface class=%d protocol=%d\n",
                 dev->subclass, dev->protocol);
    
    /* Determine if keyboard or mouse */
    if (dev->protocol == HID_PROTOCOL_KEYBOARD) {
        printk_color(TERM_GREEN, "[USB-HID] Keyboard detected\n");
        input_register_device(&usb_kbd.dev);
    } else if (dev->protocol == HID_PROTOCOL_MOUSE) {
        printk_color(TERM_GREEN, "[USB-HID] Mouse detected\n");
        input_register_device(&usb_mouse.dev);
    }
    
    return 0;
}

void usb_hid_disconnect(usb_device_t *dev)
{
    (void)dev;
    input_unregister_device(&usb_kbd.dev);
    input_unregister_device(&usb_mouse.dev);
}

/* Poll USB HID devices */
void usb_hid_poll(void)
{
    /* Poll xHCI for events */
    xhci_handle_events();
}

/* Handle keyboard report from USB HID */
void usb_hid_handle_keyboard_report(usb_kbd_report_t *report)
{
    if (!report || !usb_hid_initialized)
        return;
    
    /* Process modifier keys */
    keyboard_report_key(&usb_kbd, KEY_LEFTCTRL, (report->modifiers & USB_KBD_LCTRL) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_LEFTSHIFT, (report->modifiers & USB_KBD_LSHIFT) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_LEFTALT, (report->modifiers & USB_KBD_LALT) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_LEFTMETA, (report->modifiers & USB_KBD_LMETA) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_RIGHTCTRL, (report->modifiers & USB_KBD_RCTRL) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_RIGHTSHIFT, (report->modifiers & USB_KBD_RSHIFT) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_RIGHTALT, (report->modifiers & USB_KBD_RALT) ? KEY_PRESSED : KEY_RELEASED);
    keyboard_report_key(&usb_kbd, KEY_RIGHTMETA, (report->modifiers & USB_KBD_RMETA) ? KEY_PRESSED : KEY_RELEASED);
    
    /* Process keycodes */
    for (int i = 0; i < 6; i++) {
        if (report->keys[i] > 0) {
            uint16 keycode = usb_scancode_to_keycode(report->keys[i]);
            if (keycode != 0) {
                keyboard_report_key(&usb_kbd, keycode, KEY_PRESSED);
            }
        }
    }
    
    input_sync_device(&usb_kbd.dev);
}

/* Handle mouse report from USB HID */
void usb_hid_handle_mouse_report(usb_mouse_report_t *report)
{
    if (!report || !usb_hid_initialized)
        return;
    
    /* Buttons */
    mouse_report_button(&usb_mouse, BTN_LEFT, (report->buttons & 0x01) ? 1 : 0);
    mouse_report_button(&usb_mouse, BTN_RIGHT, (report->buttons & 0x02) ? 1 : 0);
    mouse_report_button(&usb_mouse, BTN_MIDDLE, (report->buttons & 0x04) ? 1 : 0);
    
    /* Movement */
    if (report->x != 0 || report->y != 0) {
        mouse_report_movement(&usb_mouse, report->x, report->y);
    }
    
    /* Wheel */
    if (report->wheel != 0) {
        mouse_report_wheel(&usb_mouse, report->wheel);
    }
    
    input_sync_device(&usb_mouse.dev);
}

/* Stub implementations for functions referenced in headers */
int xhci_enable_slot(uint32 *slot_id)
{
    (void)slot_id;
    return -1;
}

int xhci_address_device(uint32 slot_id, uint32 port, uint32 speed)
{
    (void)slot_id; (void)port; (void)speed;
    return -1;
}

int xhci_get_descriptor(uint32 slot_id, uint8 type, uint8 index, void *buf, uint16 len)
{
    (void)slot_id; (void)type; (void)index; (void)buf; (void)len;
    return -1;
}

int xhci_set_configuration(uint32 slot_id, uint8 config)
{
    (void)slot_id; (void)config;
    return -1;
}

int xhci_control_transfer(uint32 slot_id, uint8 req_type, uint8 request, uint16 value, uint16 index, void *data, uint16 len)
{
    (void)slot_id; (void)req_type; (void)request; (void)value; (void)index; (void)data; (void)len;
    return -1;
}

int xhci_interrupt_transfer(uint32 slot_id, uint8 ep, void *data, uint16 len)
{
    (void)slot_id; (void)ep; (void)data; (void)len;
    return -1;
}

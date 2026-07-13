/*
 * torOS USB Subsystem Header
 * xHCI Host Controller + HID driver
 */

#ifndef _USB_H
#define _USB_H

/* QEMU virt machine xHCI controller MMIO base (PCI BAR0) */
#ifndef XHCI_MMIO_BASE
#define XHCI_MMIO_BASE          0x09080000
#endif

/* USB standard constants */
#define USB_MAX_DEVICES         128
#define USB_MAX_ENDPOINTS       16
#define USB_MAX_INTERFACES      8
#define USB_MAX_CONFIGURATION_SIZE 512

/* USB speeds */
#define USB_SPEED_LOW           1
#define USB_SPEED_FULL          2
#define USB_SPEED_HIGH          3
#define USB_SPEED_SUPER         4

/* USB request types */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B

/* Descriptor types */
#define USB_DESC_DEVICE             0x01
#define USB_DESC_CONFIGURATION      0x02
#define USB_DESC_STRING             0x03
#define USB_DESC_INTERFACE          0x04
#define USB_DESC_ENDPOINT           0x05
#define USB_DESC_DEVICE_QUALIFIER   0x06
#define USB_DESC_OTHER_SPEED        0x07
#define USB_DESC_INTERFACE_POWER    0x08
#define USB_DESC_OTG                0x09
#define USB_DESC_DEBUG              0x0A
#define USB_DESC_INTERFACE_ASSOC    0x0B
#define USB_DESC_HID                0x21
#define USB_DESC_HID_REPORT         0x22

/* USB device descriptor */
typedef struct {
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 bcdUSB;
    uint8 bDeviceClass;
    uint8 bDeviceSubClass;
    uint8 bDeviceProtocol;
    uint8 bMaxPacketSize0;
    uint16 idVendor;
    uint16 idProduct;
    uint16 bcdDevice;
    uint8 iManufacturer;
    uint8 iProduct;
    uint8 iSerialNumber;
    uint8 bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

/* USB configuration descriptor */
typedef struct {
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 wTotalLength;
    uint8 bNumInterfaces;
    uint8 bConfigurationValue;
    uint8 iConfiguration;
    uint8 bmAttributes;
    uint8 bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

/* USB interface descriptor */
typedef struct {
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bInterfaceNumber;
    uint8 bAlternateSetting;
    uint8 bNumEndpoints;
    uint8 bInterfaceClass;
    uint8 bInterfaceSubClass;
    uint8 bInterfaceProtocol;
    uint8 iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

/* USB endpoint descriptor */
typedef struct {
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bEndpointAddress;
    uint8 bmAttributes;
    uint16 wMaxPacketSize;
    uint8 bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

/* USB HID descriptor */
typedef struct {
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 bcdHID;
    uint8 bCountryCode;
    uint8 bNumDescriptors;
    uint8 bDescriptorType2;
    uint16 wDescriptorLength;
} __attribute__((packed)) usb_hid_descriptor_t;

/* USB class codes */
#define USB_CLASS_HID           0x03
#define USB_CLASS_MASS_STORAGE  0x08
#define USB_CLASS_HUB           0x09

/* USB HID subclass/protocol */
#define HID_SUBCLASS_BOOT       1
#define HID_PROTOCOL_KEYBOARD   1
#define HID_PROTOCOL_MOUSE      2

/* xHCI registers */
#define XHCI_CAPLENGTH          0x00
#define XHCI_HCIVERSION         0x02
#define XHCI_HCSPARAMS1         0x04
#define XHCI_HCSPARAMS2         0x08
#define XHCI_HCSPARAMS3         0x0C
#define XHCI_HCCPARAMS1         0x10
#define XHCI_DBOFF              0x14
#define XHCI_RTSOFF             0x18

/* xHCI operational registers */
#define XHCI_USBCMD             0x00
#define XHCI_USBSTS             0x04
#define XHCI_PAGESIZE           0x08
#define XHCI_DNCTRL             0x14
#define XHCI_CRCR               0x18
#define XHCI_DCBAAP             0x30
#define XHCI_CONFIG             0x38

/* xHCI port status registers */
#define XHCI_PORTSC(n)          (0x400 + (n) * 0x10)
#define XHCI_PORTPMSC(n)        (0x404 + (n) * 0x10)
#define XHCI_PORTLI(n)          (0x408 + (n) * 0x10)
#define XHCI_PORTHLPMC(n)       (0x40C + (n) * 0x10)

/* USBCMD bits */
#define XHCI_CMD_RUN            (1 << 0)
#define XHCI_CMD_HCRESET        (1 << 1)
#define XHCI_CMD_INTE           (1 << 2)
#define XHCI_CMD_HSEE           (1 << 3)

/* USBSTS bits */
#define XHCI_STS_HCH            (1 << 0)
#define XHCI_STS_HSE            (1 << 2)
#define XHCI_STS_EINT           (1 << 3)
#define XHCI_STS_PCD            (1 << 4)
#define XHCI_STS_CNR            (1 << 11)

/* PORTSC bits */
#define XHCI_PORT_CCS           (1 << 0)
#define XHCI_PORT_PED           (1 << 1)
#define XHCI_PORT_OCA           (1 << 3)
#define XHCI_PORT_PR            (1 << 4)
#define XHCI_PORT_PP            (1 << 9)
#define XHCI_PORT_CSC           (1 << 17)
#define XHCI_PORT_PEC           (1 << 18)
#define XHCI_PORT_WRC           (1 << 19)
#define XHCI_PORT_OCC           (1 << 20)
#define XHCI_PORT_PRC           (1 << 21)
#define XHCI_PORT_PLC           (1 << 22)
#define XHCI_PORT_CEC           (1 << 23)
#define XHCI_PORT_CAS           (1 << 24)
#define XHCI_PORT_WCE           (1 << 9)

/* USB device structure */
typedef struct usb_device {
    uint8 address;
    uint8 speed;
    uint8 port;
    uint8 hub;
    uint8 class;
    uint8 subclass;
    uint8 protocol;
    uint8 max_packet0;
    uint8 num_configurations;
    uint16 vendor_id;
    uint16 product_id;
    usb_config_descriptor_t config;
    usb_interface_descriptor_t interfaces[USB_MAX_INTERFACES];
    usb_endpoint_descriptor_t endpoints[USB_MAX_ENDPOINTS];
    int interface_count;
    int endpoint_count;
    void *driver_data;
    struct usb_device *next;
} usb_device_t;

/* USB HID keyboard report */
typedef struct {
    uint8 modifiers;
    uint8 reserved;
    uint8 keys[6];
} __attribute__((packed)) usb_kbd_report_t;

/* USB HID mouse report */
typedef struct {
    uint8 buttons;
    int8 x;
    int8 y;
    int8 wheel;
} __attribute__((packed)) usb_mouse_report_t;

/* Modifier bits */
#define USB_KBD_LCTRL   0x01
#define USB_KBD_LSHIFT  0x02
#define USB_KBD_LALT    0x04
#define USB_KBD_LMETA   0x08
#define USB_KBD_RCTRL   0x10
#define USB_KBD_RSHIFT  0x20
#define USB_KBD_RALT    0x40
#define USB_KBD_RMETA   0x80

/* xHCI controller structure */
typedef struct {
    uint64 mmio_base;
    uint64 db_offset;
    uint64 rt_offset;
    uint32 page_size;
    uint32 max_slots;
    uint32 max_ports;
    uint32 *dcbaa;
    uint64 *crcr;
    usb_device_t devices[USB_MAX_DEVICES];
    int device_count;
    int initialized;
} xhci_controller_t;

/* xHCI API */
int xhci_init(uint64 mmio_base);
int xhci_reset(void);
int xhci_start(void);
void xhci_stop(void);
int xhci_port_reset(uint32 port);
int xhci_enable_slot(uint32 *slot_id);
int xhci_address_device(uint32 slot_id, uint32 port, uint32 speed);
int xhci_get_descriptor(uint32 slot_id, uint8 type, uint8 index, void *buf, uint16 len);
int xhci_set_configuration(uint32 slot_id, uint8 config);
int xhci_control_transfer(uint32 slot_id, uint8 req_type, uint8 request, uint16 value, uint16 index, void *data, uint16 len);
int xhci_interrupt_transfer(uint32 slot_id, uint8 ep, void *data, uint16 len);
void xhci_handle_events(void);
void xhci_handle_port_change(void);

/* USB HID API */
void usb_hid_init(void);
int usb_hid_probe(usb_device_t *dev);
void usb_hid_disconnect(usb_device_t *dev);
void usb_hid_poll(void);
void usb_hid_handle_keyboard_report(usb_kbd_report_t *report);
void usb_hid_handle_mouse_report(usb_mouse_report_t *report);

/* USB keyboard scan code to torOS key code */
uint16 usb_scancode_to_keycode(uint8 scancode);

/* USB hot-plug */
void usb_hotplug_init(void);
void usb_hotplug_poll(void);
void usb_hotplug_handle_connect(uint32 port);
void usb_hotplug_handle_disconnect(uint32 port);

#endif

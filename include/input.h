/*
 * torOS Input Subsystem Header
 * Event-driven input device framework
 * /dev/input/event* compatible
 */

#ifndef _INPUT_H
#define _INPUT_H

/* Input event types */
#define EV_SYN          0x00
#define EV_KEY          0x01
#define EV_REL          0x02
#define EV_ABS          0x03
#define EV_MSC          0x04
#define EV_SW           0x05
#define EV_LED          0x11
#define EV_SND          0x12
#define EV_REP          0x14
#define EV_FF           0x15
#define EV_PWR          0x16
#define EV_FF_STATUS    0x17

/* Synchronization events */
#define SYN_REPORT      0
#define SYN_CONFIG      1
#define SYN_MT_REPORT   2

/* Key event codes (partial - USB HID compatible) */
#define KEY_RESERVED    0
#define KEY_ESC         1
#define KEY_1           2
#define KEY_2           3
#define KEY_3           4
#define KEY_4           5
#define KEY_5           6
#define KEY_6           7
#define KEY_7           8
#define KEY_8           9
#define KEY_9           10
#define KEY_0           11
#define KEY_MINUS       12
#define KEY_EQUAL       13
#define KEY_BACKSPACE   14
#define KEY_TAB         15
#define KEY_Q           16
#define KEY_W           17
#define KEY_E           18
#define KEY_R           19
#define KEY_T           20
#define KEY_Y           21
#define KEY_U           22
#define KEY_I           23
#define KEY_O           24
#define KEY_P           25
#define KEY_LEFTBRACE   26
#define KEY_RIGHTBRACE  27
#define KEY_ENTER       28
#define KEY_LEFTCTRL    29
#define KEY_A           30
#define KEY_S           31
#define KEY_D           32
#define KEY_F           33
#define KEY_G           34
#define KEY_H           35
#define KEY_J           36
#define KEY_K           37
#define KEY_L           38
#define KEY_SEMICOLON   39
#define KEY_APOSTROPHE  40
#define KEY_GRAVE       41
#define KEY_LEFTSHIFT   42
#define KEY_BACKSLASH   43
#define KEY_Z           44
#define KEY_X           45
#define KEY_C           46
#define KEY_V           47
#define KEY_B           48
#define KEY_N           49
#define KEY_M           50
#define KEY_COMMA       51
#define KEY_DOT         52
#define KEY_SLASH       53
#define KEY_RIGHTSHIFT  54
#define KEY_KPASTERISK  55
#define KEY_LEFTALT     56
#define KEY_SPACE       57
#define KEY_CAPSLOCK    58
#define KEY_F1          59
#define KEY_F2          60
#define KEY_F3          61
#define KEY_F4          62
#define KEY_F5          63
#define KEY_F6          64
#define KEY_F7          65
#define KEY_F8          66
#define KEY_F9          67
#define KEY_F10         68
#define KEY_F11         87
#define KEY_F12         88
#define KEY_RIGHTCTRL   97
#define KEY_RIGHTALT    100
#define KEY_HOME        102
#define KEY_UP          103
#define KEY_PAGEUP      104
#define KEY_LEFT        105
#define KEY_RIGHT       106
#define KEY_END         107
#define KEY_DOWN        108
#define KEY_PAGEDOWN    109
#define KEY_INSERT      110
#define KEY_DELETE      111
#define KEY_LEFTMETA    125
#define KEY_RIGHTMETA   126
#define KEY_COMPOSE     127

#define KEY_MAX         256

/* Key states */
#define KEY_RELEASED    0
#define KEY_PRESSED     1
#define KEY_REPEATED    2

/* Relative axes */
#define REL_X           0x00
#define REL_Y           0x01
#define REL_Z           0x02
#define REL_RX          0x03
#define REL_RY          0x04
#define REL_RZ          0x05
#define REL_HWHEEL      0x06
#define REL_DIAL        0x07
#define REL_WHEEL       0x08
#define REL_MISC        0x09

/* Mouse buttons */
#define BTN_MOUSE       0x110
#define BTN_LEFT        0x110
#define BTN_RIGHT       0x111
#define BTN_MIDDLE      0x112
#define BTN_SIDE        0x113
#define BTN_EXTRA       0x114
#define BTN_FORWARD     0x115
#define BTN_BACK        0x116

/* Input event structure (64-bit aligned) */
typedef struct {
    uint64 time_sec;
    uint64 time_usec;
    uint16 type;
    uint16 code;
    int32 value;
} input_event_t;

/* Input device descriptor */
typedef struct input_dev {
    char name[32];
    uint32 id_vendor;
    uint32 id_product;
    uint32 evbit[4];        /* Supported event types bitmap */
    uint32 keybit[8];       /* Supported key codes bitmap */
    uint32 relbit[2];       /* Supported relative axes bitmap */
    uint32 absbit[2];       /* Supported absolute axes bitmap */
    void *private;          /* Driver private data */
    int (*open)(struct input_dev *dev);
    void (*close)(struct input_dev *dev);
    void (*event)(struct input_dev *dev, uint16 type, uint16 code, int32 value);
    struct input_dev *next;
} input_dev_t;

/* Input handler */
typedef struct input_handler {
    void (*connect)(struct input_dev *dev);
    void (*disconnect)(struct input_dev *dev);
    void (*event)(struct input_dev *dev, input_event_t *evt);
    uint32 evbit[4];        /* Interested event types */
    struct input_handler *next;
} input_handler_t;

/* Event ring buffer */
#define INPUT_EVENT_BUFFER_SIZE 256

typedef struct {
    input_event_t events[INPUT_EVENT_BUFFER_SIZE];
    volatile uint32 head;
    volatile uint32 tail;
    spinlock_t lock;
} input_event_buffer_t;

/* Global input subsystem state */
typedef struct {
    input_dev_t *devices;
    input_handler_t *handlers;
    input_event_buffer_t global_buffer;
    spinlock_t subsystem_lock;
    int initialized;
} input_subsystem_t;

/* Input subsystem API */
void input_subsystem_init(void);
int input_register_device(input_dev_t *dev);
void input_unregister_device(input_dev_t *dev);
int input_register_handler(input_handler_t *handler);
void input_unregister_handler(input_handler_t *handler);
void input_report_event(input_dev_t *dev, uint16 type, uint16 code, int32 value);
void input_sync_device(input_dev_t *dev);

/* Event buffer API */
int input_event_read(input_event_buffer_t *buf, input_event_t *evt);
int input_event_write(input_event_buffer_t *buf, input_event_t *evt);
int input_event_available(input_event_buffer_t *buf);
void input_event_buffer_init(input_event_buffer_t *buf);
int input_read_event(input_event_t *evt);

/* Keyboard specific */
typedef struct {
    input_dev_t dev;
    uint8 key_state[KEY_MAX / 8 + 1];
    uint8 prev_key_state[KEY_MAX / 8 + 1];
    uint32 modifiers;
} keyboard_device_t;

#define MOD_LSHIFT  (1 << 0)
#define MOD_RSHIFT  (1 << 1)
#define MOD_LCTRL   (1 << 2)
#define MOD_RCTRL   (1 << 3)
#define MOD_LALT    (1 << 4)
#define MOD_RALT    (1 << 5)
#define MOD_LMETA   (1 << 6)
#define MOD_RMETA   (1 << 7)
#define MOD_CAPS    (1 << 8)

void keyboard_report_key(keyboard_device_t *kbd, uint16 code, int32 value);
int keyboard_is_key_pressed(keyboard_device_t *kbd, uint16 code);
char keyboard_scancode_to_ascii(keyboard_device_t *kbd, uint16 code);

/* Mouse specific */
typedef struct {
    input_dev_t dev;
    int32 x;
    int32 y;
    int32 wheel;
    uint32 button_state;
    uint32 screen_width;
    uint32 screen_height;
    uint32 accel_numerator;
    uint32 accel_denominator;
    uint32 threshold;
} mouse_device_t;

void mouse_report_movement(mouse_device_t *mouse, int32 dx, int32 dy);
void mouse_report_button(mouse_device_t *mouse, uint32 button, int32 pressed);
void mouse_report_wheel(mouse_device_t *mouse, int32 delta);
void mouse_set_bounds(mouse_device_t *mouse, uint32 width, uint32 height);

#endif

/*
 * virtio_input.h — virtio-input aygıt protokolü (Faz 1.1/1.2)
 */
#ifndef TOROS_VIRTIO_INPUT_H
#define TOROS_VIRTIO_INPUT_H

#include <toros/types.h>

/* PCI device ID'leri: legacy transitional 0x1000+18, modern 0x1040+18 */
#define VIRTIO_INPUT_DEVICE_ID_LEGACY 0x1012
#define VIRTIO_INPUT_DEVICE_ID_MODERN 0x1052

/* Config select değerleri */
#define VIRTIO_INPUT_CFG_UNSET    0x00
#define VIRTIO_INPUT_CFG_ID_NAME  0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL 0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS 0x03
#define VIRTIO_INPUT_CFG_PROP_BITS 0x10
#define VIRTIO_INPUT_CFG_EV_BITS  0x11
#define VIRTIO_INPUT_CFG_ABS_INFO 0x12

/* Plan: event formatı */
struct virtio_input_event {
    u16 type;   /* EV_KEY=1, EV_REL=2, EV_ABS=3 */
    u16 code;   /* KEY_A=30, KEY_ENTER=28, ... */
    u32 value;  /* 0=release, 1=press, 2=repeat */
} __attribute__((packed));

void virtio_input_init(void);

#endif

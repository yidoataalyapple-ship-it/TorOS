/*
 * torOS VirtIO Header
 * PCI-based VirtIO device framework
 */

#ifndef _VIRTIO_H
#define _VIRTIO_H

/* VirtIO PCI constants */
#define VIRTIO_PCI_VENDOR_ID    0x1AF4
#define VIRTIO_PCI_DEVICE_MIN   0x1000
#define VIRTIO_PCI_DEVICE_MAX   0x107F

/* QEMU PCI config space */
#define PCI_CONFIG_ADDR         0x3D4
#define PCI_CONFIG_DATA         0x3D8

/* PCI ECAM for QEMU virt */
#define PCI_ECAM_BASE           0x3F000000
#define PCI_ECAM_SIZE           0x01000000

/* VirtIO legacy PCI layout */
#define VIRTIO_PCI_HOST_FEATURES  0x00
#define VIRTIO_PCI_GUEST_FEATURES 0x04
#define VIRTIO_PCI_QUEUE_PFN      0x08
#define VIRTIO_PCI_QUEUE_NUM      0x0C
#define VIRTIO_PCI_QUEUE_SEL      0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY   0x10
#define VIRTIO_PCI_STATUS         0x12
#define VIRTIO_PCI_ISR            0x13
#define VIRTIO_PCI_DEVICE_SPECIFIC 0x14

/* VirtIO status flags */
#define VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 0x40
#define VIRTIO_STATUS_FAILED        0x80

/* VirtIO device IDs */
#define VIRTIO_ID_NET           1
#define VIRTIO_ID_BLOCK         2
#define VIRTIO_ID_CONSOLE       3
#define VIRTIO_ID_RNG           4
#define VIRTIO_ID_BALLOON       5
#define VIRTIO_ID_RPMSG         7
#define VIRTIO_ID_SCSI          8
#define VIRTIO_ID_9P            9
#define VIRTIO_ID_RPROC_SERIAL  11
#define VIRTIO_ID_CAIF          12
#define VIRTIO_ID_GPU           16
#define VIRTIO_ID_INPUT         18
#define VIRTIO_ID_VSOCK         19
#define VIRTIO_ID_CRYPTO        20
#define VIRTIO_ID_SOUND         25

/* VirtIO ring descriptor flags */
#define VRING_DESC_F_NEXT       1
#define VRING_DESC_F_WRITE      2
#define VRING_DESC_F_INDIRECT   4

/* VirtIO feature bits */
#define VIRTIO_F_ANY_LAYOUT     27
#define VIRTIO_F_VERSION_1      32
#define VIRTIO_F_ACCESS_PLATFORM 33
#define VIRTIO_F_RING_PACKED    34
#define VIRTIO_F_IN_ORDER       35
#define VIRTIO_F_ORDER_PLATFORM 36
#define VIRTIO_F_SR_IOV         37
#define VIRTIO_F_NOTIFICATION_DATA 38

/* VirtQueue descriptor */
typedef struct {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
} vring_desc_t;

/* VirtQueue available ring */
typedef struct {
    uint16 flags;
    uint16 idx;
    uint16 ring[];
} vring_avail_t;

/* VirtQueue used element */
typedef struct {
    uint32 id;
    uint32 len;
} vring_used_elem_t;

/* VirtQueue used ring */
typedef struct {
    uint16 flags;
    uint16 idx;
    vring_used_elem_t ring[];
} vring_used_t;

/* VirtQueue structure */
typedef struct {
    uint32 queue_index;
    uint16 queue_size;
    uint16 free_num;
    uint16 free_head;
    uint16 last_used_idx;
    vring_desc_t *desc;
    vring_avail_t *avail;
    vring_used_t *used;
    uint32 notify_offset;
    volatile uint32 *notify_reg;
    void *queue_pages;
} virtqueue_t;

/* VirtIO device structure */
typedef struct virtio_device {
    uint32 pci_bus;
    uint32 pci_slot;
    uint32 pci_func;
    uint32 device_id;
    uint32 vendor_id;
    uint32 io_base;
    uint64 features;
    uint32 status;
    virtqueue_t queues[8];
    uint32 num_queues;
    void *device_config;
    void (*interrupt_handler)(struct virtio_device *dev);
    void *driver_data;
    struct virtio_device *next;
} virtio_device_t;

/* VirtIO input specific */
#define VIRTIO_INPUT_CFG_UNSET      0x00
#define VIRTIO_INPUT_CFG_ID_NAME    0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL  0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS  0x03
#define VIRTIO_INPUT_CFG_PROP_BITS  0x10
#define VIRTIO_INPUT_CFG_EV_BITS    0x11
#define VIRTIO_INPUT_CFG_ABS_INFO   0x12

typedef struct {
    uint16 type;
    uint16 code;
    int32 value;
} virtio_input_event_t;

typedef struct {
    uint8 select;
    uint8 subsel;
    uint8 size;
    uint8 reserved[5];
    union {
        char string[128];
        uint8 bitmap[128];
        struct {
            uint16 bustype;
            uint16 vendor;
            uint16 product;
            uint16 version;
        } ids;
        struct {
            int32 min;
            int32 max;
            int32 fuzz;
            int32 flat;
            int32 res;
        } abs;
    } u;
} virtio_input_config_t;

/* PCI API */
void pci_init(void);
uint32 pci_read_config(uint32 bus, uint32 slot, uint32 func, uint32 offset);
void pci_write_config(uint32 bus, uint32 slot, uint32 func, uint32 offset, uint32 val);
int pci_find_device(uint16 vendor_id, uint16 device_id, uint32 *bus, uint32 *slot, uint32 *func);
int pci_find_virtio_device(uint32 virtio_id, virtio_device_t *vdev);

/* VirtIO core API */
void virtio_init(void);
int virtio_device_init(virtio_device_t *dev);
void virtio_write_status(virtio_device_t *dev, uint8 status);
uint32 virtio_get_features(virtio_device_t *dev);
void virtio_set_features(virtio_device_t *dev, uint32 features);
int virtio_setup_queue(virtio_device_t *dev, uint16 queue_idx, virtqueue_t *vq);
void virtio_notify_queue(virtio_device_t *dev, virtqueue_t *vq);
void virtio_disable_interrupts(virtqueue_t *vq);
void virtio_enable_interrupts(virtqueue_t *vq);
int virtqueue_add_buffers(virtqueue_t *vq, void *bufs[], uint32 lens[], uint32 flags_list[], int count);
int virtqueue_get_buffer(virtqueue_t *vq, uint32 *len);
void virtqueue_kick(virtqueue_t *vq);

/* VirtIO input API */
void virtio_input_init(void);
void virtio_input_handle_interrupt(virtio_device_t *dev);
int virtio_input_has_event(void);
virtio_input_event_t virtio_input_get_event(void);
void virtio_input_get_config(uint8 select, uint8 subsel, virtio_input_config_t *cfg);

#endif

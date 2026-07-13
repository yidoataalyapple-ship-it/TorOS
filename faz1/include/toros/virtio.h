/*
 * virtio.h — Legacy (0.9.x uyumlu) virtio-pci çekirdeği + vring
 *
 * Plan referansı: Faz 1.1 — "Register map: virtio-input device config
 * space (PCI BAR0)". QEMU'nun transitional virtio-*-pci aygıtları
 * legacy I/O BAR0 arayüzünü sağlar; bu sürücü o arayüzü kullanır.
 */
#ifndef TOROS_VIRTIO_H
#define TOROS_VIRTIO_H

#include <toros/types.h>
#include <toros/pci.h>

/* Legacy virtio-pci I/O register offsetleri (BAR0 göreli) */
#define VIRTIO_PCI_HOST_FEATURES  0x00   /* R, 32 */
#define VIRTIO_PCI_GUEST_FEATURES 0x04   /* W, 32 */
#define VIRTIO_PCI_QUEUE_PFN      0x08   /* RW, 32 */
#define VIRTIO_PCI_QUEUE_NUM      0x0C   /* R, 16 */
#define VIRTIO_PCI_QUEUE_SEL      0x0E   /* RW, 16 */
#define VIRTIO_PCI_QUEUE_NOTIFY   0x10   /* W, 16 */
#define VIRTIO_PCI_STATUS         0x12   /* RW, 8 */
#define VIRTIO_PCI_ISR            0x13   /* R, 8 */
#define VIRTIO_PCI_CONFIG_OFF(msix) ((msix) ? 0x18 : 0x14)

/* Legacy ISR bitleri */
#define VIRTIO_ISR_QUEUE  BIT(0)
#define VIRTIO_ISR_CONFIG BIT(1)

/* Device status bitleri */
#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FAILED    128

/* vring tanımları (legacy düzen) */
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct vring_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed));

struct vring_avail {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed));

struct vring_used_elem {
    u32 id;
    u32 len;
} __attribute__((packed));

struct vring_used {
    u16 flags;
    u16 idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

/* Modern (1.0) common config register offsetleri */
#define VIRTIO_COMMON_DFSELECT   0x00
#define VIRTIO_COMMON_DF         0x04
#define VIRTIO_COMMON_GFSELECT   0x08
#define VIRTIO_COMMON_GF         0x0C
#define VIRTIO_COMMON_MSIX       0x10
#define VIRTIO_COMMON_NUMQ       0x12
#define VIRTIO_COMMON_STATUS     0x14
#define VIRTIO_COMMON_CFGGEN     0x15
#define VIRTIO_COMMON_Q_SELECT   0x16
#define VIRTIO_COMMON_Q_SIZE     0x18
#define VIRTIO_COMMON_Q_MSIX     0x1A
#define VIRTIO_COMMON_Q_ENABLE   0x1C
#define VIRTIO_COMMON_Q_NOFF     0x1E
#define VIRTIO_COMMON_Q_DESC     0x20
#define VIRTIO_COMMON_Q_DRIVER   0x28
#define VIRTIO_COMMON_Q_DEVICE   0x30

/* Status bitleri */
#define VIRTIO_STATUS_FEATURES_OK 8

/* PCI vendor capability cfg_type değerleri */
#define VIRTIO_PCI_CAP_COMMON 1
#define VIRTIO_PCI_CAP_NOTIFY 2
#define VIRTIO_PCI_CAP_ISR    3
#define VIRTIO_PCI_CAP_DEVICE 4

#define VIRTIO_F_VERSION_1 32

#define VIRTIO_MAX_QUEUES 4

struct virtqueue {
    u16 index;
    u16 size;                 /* ring eleman sayısı (2^n) */
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    u64  mem;                 /* vring bellek tabanı */
    u32  mem_size;
    u64  notify_addr;         /* notify yazma adresi */
    u16  last_used;           /* son işlenen used idx */
    u16  num_free;            /* boş descriptor sayısı */
    u16  free_head;           /* boş descriptor zinciri başı */
    struct virtio_dev *dev;
};

struct virtio_dev {
    const struct pci_device *pci;
    int  modern;              /* 1 = virtio 1.0 capability transport */
    /* Legacy */
    u64 iobase;               /* legacy BAR0 efektif adresi */
    u64 cfgbase;              /* device-specific config tabanı */
    /* Modern */
    u64 common_cfg;
    u64 notify_base;
    u32 notify_mult;
    u64 isr_addr;
    u16 pci_device_id;
    u32 intid;                /* GIC INTID (32 + irq_line) */
    struct virtqueue vq[VIRTIO_MAX_QUEUES];
    int  nvq;
};

/* Transport katmanı: modern (capability) önce, legacy fallback */
int  virtio_transport_init(struct virtio_dev *vd, const struct pci_device *pci);
int  virtio_legacy_init(struct virtio_dev *vd, const struct pci_device *pci);
int  virtio_modern_init(struct virtio_dev *vd, const struct pci_device *pci);
u32  virtio_get_features(struct virtio_dev *vd);
void virtio_set_features(struct virtio_dev *vd, u32 features);
int  virtio_setup_queue(struct virtio_dev *vd, u16 qidx);
void virtio_driver_ok(struct virtio_dev *vd);
void virtio_reset(struct virtio_dev *vd);
u32  virtio_read_isr(struct virtio_dev *vd);

/* vring işlemleri */
/* Tek buffer ekle (write=1: device yazacak). Descriptor id döner, -1 hata. */
int  vq_add_buf(struct virtqueue *vq, u64 addr, u32 len, int write);
void vq_kick(struct virtqueue *vq);
/* Kullanılmış buffer al: id döner, *len device yazım uzunluğu; -1 yok */
int  vq_get_used(struct virtqueue *vq, u32 *len);
void vq_free_desc(struct virtqueue *vq, u16 id);
int  vq_has_used(struct virtqueue *vq);

#endif

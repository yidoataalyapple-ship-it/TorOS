/*
 * virtio.c — virtio-pci transport: modern (1.0 capabilities) + legacy fallback
 *
 * Modern: PCI vendor capability'leri ile common/notify/isr/device config
 * bölgeleri bulunur; queue'lar ayrık desc/driver/device adresleriyle kurulur.
 *
 * Legacy: BAR0 I/O register arayüzü (tek PFN tabanlı vring).
 */
#include <toros/virtio.h>
#include <toros/mm.h>
#include <toros/printf.h>
#include <toros/string.h>

/* ---------------- Legacy register erişimi ---------------- */
static inline u8  vp_r8(struct virtio_dev *d, u32 off)  { return mmio_read8(d->iobase + off); }
static inline u16 vp_r16(struct virtio_dev *d, u32 off) { return mmio_read16(d->iobase + off); }
static inline u32 vp_r32(struct virtio_dev *d, u32 off) { return mmio_read32(d->iobase + off); }
static inline void vp_w8(struct virtio_dev *d, u32 off, u8 v)   { mmio_write8(d->iobase + off, v); }
static inline void vp_w16(struct virtio_dev *d, u32 off, u16 v) { mmio_write16(d->iobase + off, v); }
static inline void vp_w32(struct virtio_dev *d, u32 off, u32 v) { mmio_write32(d->iobase + off, v); }

/* ---------------- Modern register erişimi ---------------- */
static inline u8  mc_r8(struct virtio_dev *d, u32 off)  { return mmio_read8(d->common_cfg + off); }
static inline u16 mc_r16(struct virtio_dev *d, u32 off) { return mmio_read16(d->common_cfg + off); }
static inline void mc_w8(struct virtio_dev *d, u32 off, u8 v)   { mmio_write8(d->common_cfg + off, v); }
static inline void mc_w16(struct virtio_dev *d, u32 off, u16 v) { mmio_write16(d->common_cfg + off, v); }
static inline void mc_w32(struct virtio_dev *d, u32 off, u32 v) { mmio_write32(d->common_cfg + off, v); }

static u8 dev_status(struct virtio_dev *vd)
{
    return vd->modern ? mc_r8(vd, VIRTIO_COMMON_STATUS)
                      : vp_r8(vd, VIRTIO_PCI_STATUS);
}

static void set_status(struct virtio_dev *vd, u8 st)
{
    if (vd->modern)
        mc_w8(vd, VIRTIO_COMMON_STATUS, st);
    else
        vp_w8(vd, VIRTIO_PCI_STATUS, st);
}

void virtio_reset(struct virtio_dev *vd)
{
    set_status(vd, 0);
    for (int i = 0; i < 200000; i++) {
        if (dev_status(vd) == 0)
            break;
        nop();
    }
}

/* ================= Legacy transport ================= */

int virtio_legacy_init(struct virtio_dev *vd, const struct pci_device *pci)
{
    memset(vd, 0, sizeof(*vd));
    vd->pci = pci;
    vd->pci_device_id = pci->device;
    vd->modern = 0;

    u64 bar0 = pci_bar_address(pci, 0);
    if (bar0 == 0 || !pci->bar_is_io[0])
        return -1;

    vd->iobase = bar0;
    vd->cfgbase = bar0 + VIRTIO_PCI_CONFIG_OFF(pci->has_msix);
    vd->isr_addr = bar0 + VIRTIO_PCI_ISR;
    vd->intid = 32 + pci->irq_line;

    virtio_reset(vd);
    set_status(vd, VIRTIO_STATUS_ACK);
    set_status(vd, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    return 0;
}

/* ================= Modern transport ================= */

static int find_capability(struct virtio_dev *vd, u8 cfg_type,
                           u64 *out_addr, u32 *out_aux)
{
    const struct pci_device *pci = vd->pci;
    u16 status = pci_cfg_read16(pci, PCI_REG_STATUS);
    if (!(status & BIT(4)))
        return -1;

    u8 cap = pci_cfg_read8(pci, PCI_REG_CAPPTR);
    while (cap && cap != 0xFF) {
        u8 id = pci_cfg_read8(pci, cap);
        if (id == 0x09) {  /* vendor specific */
            u8 type = pci_cfg_read8(pci, cap + 3);
            if (type == cfg_type) {
                u8 bar_idx = pci_cfg_read8(pci, cap + 4);
                u32 off = pci_cfg_read32(pci, cap + 8);
                u64 bar = pci_bar_address(pci, bar_idx);
                if (bar == 0)
                    return -1;
                *out_addr = bar + off;
                if (out_aux)
                    *out_aux = (cfg_type == VIRTIO_PCI_CAP_NOTIFY)
                             ? pci_cfg_read32(pci, cap + 16) : 0;
                return 0;
            }
        }
        cap = pci_cfg_read8(pci, cap + 1);
    }
    return -1;
}

int virtio_modern_init(struct virtio_dev *vd, const struct pci_device *pci)
{
    memset(vd, 0, sizeof(*vd));
    vd->pci = pci;
    vd->pci_device_id = pci->device;
    vd->modern = 1;
    vd->intid = 32 + pci->irq_line;

    if (find_capability(vd, VIRTIO_PCI_CAP_COMMON, &vd->common_cfg, NULL) != 0)
        return -1;
    if (find_capability(vd, VIRTIO_PCI_CAP_NOTIFY, &vd->notify_base, &vd->notify_mult) != 0)
        return -1;
    if (find_capability(vd, VIRTIO_PCI_CAP_ISR, &vd->isr_addr, NULL) != 0)
        return -1;
    if (find_capability(vd, VIRTIO_PCI_CAP_DEVICE, &vd->cfgbase, NULL) != 0)
        return -1;

    /* Reset + status makinesi */
    virtio_reset(vd);
    set_status(vd, VIRTIO_STATUS_ACK);
    set_status(vd, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    /* Feature pazarlığı: VIRTIO_F_VERSION_1 (bit 32 -> select 1, bit 0) */
    mc_w32(vd, VIRTIO_COMMON_DFSELECT, 1);
    u32 f_hi = mmio_read32(vd->common_cfg + VIRTIO_COMMON_DF);
    if (!(f_hi & 1)) {
        kerr("virtio: modern aygıt VERSION_1 sunmuyor\n");
        set_status(vd, VIRTIO_STATUS_FAILED);
        return -1;
    }
    mc_w32(vd, VIRTIO_COMMON_GFSELECT, 1);
    mc_w32(vd, VIRTIO_COMMON_GF, 1);
    mc_w32(vd, VIRTIO_COMMON_GFSELECT, 0);
    mc_w32(vd, VIRTIO_COMMON_GF, 0);   /* alt 32 bit: hiçbir özellik */

    set_status(vd, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    if (!(dev_status(vd) & VIRTIO_STATUS_FEATURES_OK)) {
        kerr("virtio: FEATURES_OK reddedildi\n");
        set_status(vd, VIRTIO_STATUS_FAILED);
        return -1;
    }
    return 0;
}

/* Birleşik giriş: önce modern, sonra legacy */
int virtio_transport_init(struct virtio_dev *vd, const struct pci_device *pci)
{
    if (virtio_modern_init(vd, pci) == 0)
        return 0;
    return virtio_legacy_init(vd, pci);
}

/* ================= Ortak katman ================= */

u32 virtio_get_features(struct virtio_dev *vd)
{
    if (vd->modern) {
        mc_w32(vd, VIRTIO_COMMON_DFSELECT, 0);
        return mmio_read32(vd->common_cfg + VIRTIO_COMMON_DF);
    }
    return vp_r32(vd, VIRTIO_PCI_HOST_FEATURES);
}

void virtio_set_features(struct virtio_dev *vd, u32 features)
{
    if (vd->modern) {
        mc_w32(vd, VIRTIO_COMMON_GFSELECT, 0);
        mc_w32(vd, VIRTIO_COMMON_GF, features);
        return;
    }
    vp_w32(vd, VIRTIO_PCI_GUEST_FEATURES, features);
}

int virtio_setup_queue(struct virtio_dev *vd, u16 qidx)
{
    if (qidx >= VIRTIO_MAX_QUEUES)
        return -1;

    struct virtqueue *vq = &vd->vq[qidx];
    u16 size;

    if (vd->modern) {
        mc_w16(vd, VIRTIO_COMMON_Q_SELECT, qidx);
        size = mc_r16(vd, VIRTIO_COMMON_Q_SIZE);
        if (size == 0)
            return -1;

        /* Ayrık bölgeler: desc / avail / used */
        u32 desc_sz = 16 * size;
        u32 avail_sz = 4 + 2 * size + 2;
        u32 used_sz = 4 + 8 * size + 2;

        void *dmem = alloc_pages(desc_sz);
        void *amem = alloc_pages(avail_sz);
        void *umem = alloc_pages(used_sz);
        if (!dmem || !amem || !umem)
            return -1;

        vq->desc = dmem;
        vq->avail = amem;
        vq->used = umem;
        vq->mem = (u64)dmem;
        vq->mem_size = desc_sz;

        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DESC, (u32)(u64)dmem);
        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DESC + 4, (u32)((u64)dmem >> 32));
        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DRIVER, (u32)(u64)amem);
        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DRIVER + 4, (u32)((u64)amem >> 32));
        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DEVICE, (u32)(u64)umem);
        mmio_write32(vd->common_cfg + VIRTIO_COMMON_Q_DEVICE + 4, (u32)((u64)umem >> 32));

        u16 noff = mc_r16(vd, VIRTIO_COMMON_Q_NOFF);
        vq->notify_addr = vd->notify_base + (u64)noff * vd->notify_mult;

        mc_w16(vd, VIRTIO_COMMON_Q_ENABLE, 1);
    } else {
        vp_w16(vd, VIRTIO_PCI_QUEUE_SEL, qidx);
        size = vp_r16(vd, VIRTIO_PCI_QUEUE_NUM);
        if (size == 0)
            return -1;

        u32 avail_sz = 4 + 2 * size + 2;
        u32 used_off = (u32)ALIGN_UP(16 * size + avail_sz, 4096);
        u32 used_sz = 4 + 8 * size + 2;
        u32 total = used_off + used_sz;

        void *mem = alloc_pages(total);
        if (!mem)
            return -1;

        vq->desc = mem;
        vq->avail = (struct vring_avail *)((u8 *)mem + 16 * size);
        vq->used = (struct vring_used *)((u8 *)mem + used_off);
        vq->mem = (u64)mem;
        vq->mem_size = total;
        vq->notify_addr = vd->iobase + VIRTIO_PCI_QUEUE_NOTIFY;

        vp_w32(vd, VIRTIO_PCI_QUEUE_PFN, (u32)((u64)mem >> 12));
    }

    vq->index = qidx;
    vq->size = size;
    vq->last_used = 0;
    vq->dev = vd;

    for (u16 i = 0; i < size; i++) {
        vq->desc[i].next = i + 1;
        vq->desc[i].flags = 0;
    }
    vq->desc[size - 1].next = 0xFFFF;
    vq->free_head = 0;
    vq->num_free = size;

    if (qidx >= vd->nvq)
        vd->nvq = qidx + 1;
    return 0;
}

void virtio_driver_ok(struct virtio_dev *vd)
{
    set_status(vd, dev_status(vd) | VIRTIO_STATUS_DRIVER_OK);
}

u32 virtio_read_isr(struct virtio_dev *vd)
{
    return mmio_read8(vd->isr_addr);
}

/* ---------------- vring işlemleri (ortak) ---------------- */

int vq_add_buf(struct virtqueue *vq, u64 addr, u32 len, int write)
{
    if (vq->num_free == 0)
        return -1;

    u16 id = vq->free_head;
    vq->free_head = vq->desc[id].next;
    vq->num_free--;

    vq->desc[id].addr = addr;
    vq->desc[id].len = len;
    vq->desc[id].flags = write ? VRING_DESC_F_WRITE : 0;
    vq->desc[id].next = 0;

    u16 aidx = vq->avail->idx;
    vq->avail->ring[aidx % vq->size] = id;
    dsb_sy();
    vq->avail->idx = aidx + 1;
    dsb_sy();
    return id;
}

void vq_kick(struct virtqueue *vq)
{
    dsb_sy();
    mmio_write16(vq->notify_addr, vq->index);
}

int vq_has_used(struct virtqueue *vq)
{
    return vq->used->idx != vq->last_used;
}

int vq_get_used(struct virtqueue *vq, u32 *len)
{
    if (vq->used->idx == vq->last_used)
        return -1;

    dsb_sy();
    struct vring_used_elem e = vq->used->ring[vq->last_used % vq->size];
    vq->last_used++;
    if (len)
        *len = e.len;
    return (int)e.id;
}

void vq_free_desc(struct virtqueue *vq, u16 id)
{
    vq->desc[id].next = vq->free_head;
    vq->free_head = id;
    vq->num_free++;
}

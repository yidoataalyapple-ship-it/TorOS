/*
 * virtio_input.c — virtio-input sürücüsü (Faz 1.1 + 1.2)
 *
 * QEMU: -device virtio-keyboard-pci / -device virtio-mouse-pci
 * Modern (1.0) veya transitional (legacy) PCI aygıtı; transport
 * katmanı her iki modu da destekler.
 *
 * Config space (cfgbase göreli):
 *   +0 select (u8), +1 subsel (u8), +2 size (u8, RO), +8 data (128B)
 * Queue 0 = eventq (device->driver), Queue 1 = statusq (driver->device)
 */
#include <toros/virtio_input.h>
#include <toros/virtio.h>
#include <toros/pci.h>
#include <toros/input.h>
#include <toros/keyboard.h>
#include <toros/mouse.h>
#include <toros/mm.h>
#include <toros/gic.h>
#include <toros/irq.h>
#include <toros/printf.h>
#include <toros/string.h>

#define MAX_VINPUT 4
#define EVENTQ 0
#define STATUSQ 1

/* Her eventq buffer'ı tek event tutar */
#define EVBUF_COUNT 32

enum vinput_kind {
    VINPUT_UNKNOWN = 0,
    VINPUT_KEYBOARD,
    VINPUT_MOUSE,
};

struct vinput {
    struct virtio_dev vd;
    enum vinput_kind kind;
    char name[64];
    /* eventq buffer'ları */
    struct virtio_input_event *bufs[EVBUF_COUNT];
    int nbufs;
    int active;
};

static struct vinput devices[MAX_VINPUT];
static int ndev;

/* Config alanı okuma yardımcıları */
static u8 cfg_read8(struct vinput *vi, u32 off)
{
    return mmio_read8(vi->vd.cfgbase + off);
}

static void cfg_write8(struct vinput *vi, u32 off, u8 v)
{
    mmio_write8(vi->vd.cfgbase + off, v);
}

static int cfg_query(struct vinput *vi, u8 select, u8 subsel, u8 *out, int max)
{
    cfg_write8(vi, 0, select);
    cfg_write8(vi, 1, subsel);
    dsb_sy();
    u8 size = cfg_read8(vi, 2);
    if (size == 0 || select == 0)
        return 0;
    int n = size < max ? size : max;
    for (int i = 0; i < n; i++)
        out[i] = cfg_read8(vi, 8 + i);
    return size;
}

/* Linux input.h ile uyumlu birkaç KEY kodu (klavye tespiti) */
#define KEY_A 30
#define KEY_Z 44

static void vinput_classify(struct vinput *vi)
{
    u8 keybits[96] = {0};
    u8 relbits[4] = {0};

    /*
     * Not: QEMU, EV_BITS subsel=0 (olay tipi bitmap'i) için size=0
     * döndürür; bu yüzden her tipi doğrudan sorguluyoruz.
     */
    int key_sz = cfg_query(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_KEY, keybits, sizeof(keybits));
    int rel_sz = cfg_query(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_REL, relbits, sizeof(relbits));

    int has_letters = input_test_bit(keybits, KEY_A) && input_test_bit(keybits, KEY_Z);
    int has_rel_xy = input_test_bit(relbits, REL_X) && input_test_bit(relbits, REL_Y);
    int has_buttons = input_test_bit(keybits, BTN_LEFT);

    if (has_letters)
        vi->kind = VINPUT_KEYBOARD;
    else if (has_rel_xy && has_buttons)
        vi->kind = VINPUT_MOUSE;
    else
        vi->kind = VINPUT_UNKNOWN;

    kinfo("virtio-input classify: key_sz=%d rel_sz=%d letters=%d xy=%d btn=%d -> %d\n",
          key_sz, rel_sz, has_letters, has_rel_xy, has_buttons, vi->kind);
}

/* ---------------- Event işleme ---------------- */

static void vinput_process_event(struct vinput *vi, const struct virtio_input_event *ev)
{
    switch (vi->kind) {
    case VINPUT_KEYBOARD:
        if (ev->type == EV_KEY)
            keyboard_handle_key(ev->code, (s32)ev->value);
        break;
    case VINPUT_MOUSE:
        if (ev->type == EV_REL)
            mouse_handle_rel(ev->code, (s32)(s16)ev->value);
        else if (ev->type == EV_KEY)
            mouse_handle_button(ev->code, (s32)ev->value);
        break;
    default:
        break;
    }
}

static void vinput_drain(struct vinput *vi)
{
    struct virtqueue *vq = &vi->vd.vq[EVENTQ];
    u32 len;
    int id;

    while ((id = vq_get_used(vq, &len)) >= 0) {
        if (id < vi->nbufs) {
            struct virtio_input_event *ev = vi->bufs[id];
            /* buffer birden çok event içerebilir */
            u32 count = len / sizeof(struct virtio_input_event);
            for (u32 i = 0; i < count; i++)
                vinput_process_event(vi, &ev[i]);
            /* Buffer'ı geri ver */
            vq_free_desc(vq, (u16)id);
            vq_add_buf(vq, (u64)ev, sizeof(struct virtio_input_event), 1);
        } else {
            vq_free_desc(vq, (u16)id);
        }
    }
    vq_kick(vq);
}

static void vinput_irq(u32 intid, void *arg)
{
    (void)intid;
    struct vinput *vi = arg;

    u32 isr = virtio_read_isr(&vi->vd);   /* okuma = ack */
    if (isr & VIRTIO_ISR_QUEUE)
        vinput_drain(vi);
}

/* IRQ çalışmazsa diye timer'dan periyodik yedek tarama */
void virtio_input_poll_all(void)
{
    for (int i = 0; i < ndev; i++) {
        if (devices[i].active && vq_has_used(&devices[i].vd.vq[EVENTQ]))
            vinput_drain(&devices[i]);
    }
}

/* ---------------- Kurulum ---------------- */

static int vinput_init_one(const struct pci_device *pci)
{
    if (ndev >= MAX_VINPUT)
        return -1;

    struct vinput *vi = &devices[ndev];

    if (virtio_transport_init(&vi->vd, pci) != 0)
        return -1;

    u32 feat = virtio_get_features(&vi->vd);
    if (!vi->vd.modern)
        virtio_set_features(&vi->vd, 0);

    /* Queues: eventq + statusq */
    if (virtio_setup_queue(&vi->vd, EVENTQ) != 0) {
        kerr("virtio-input: eventq kurulamadı\n");
        return -1;
    }
    if (virtio_setup_queue(&vi->vd, STATUSQ) != 0) {
        kerr("virtio-input: statusq kurulamadı\n");
        return -1;
    }

    /* İsim ve sınıflandırma */
    memset(vi->name, 0, sizeof(vi->name));
    int n = cfg_query(vi, VIRTIO_INPUT_CFG_ID_NAME, 0, (u8 *)vi->name, sizeof(vi->name) - 1);
    if (n <= 0)
        strncpy(vi->name, "virtio-input", sizeof(vi->name) - 1);

    vinput_classify(vi);

    /* Eventq buffer'larını doldur */
    struct virtqueue *vq = &vi->vd.vq[EVENTQ];
    vi->nbufs = vq->size < EVBUF_COUNT ? vq->size : EVBUF_COUNT;
    for (int i = 0; i < vi->nbufs; i++) {
        vi->bufs[i] = alloc_page();   /* 4KB, DMA uyumlu */
        vq_add_buf(vq, (u64)vi->bufs[i], sizeof(struct virtio_input_event), 1);
    }
    vq_kick(vq);

    /* IRQ kaydet + enable */
    irq_register(vi->vd.intid, vinput_irq, vi);
    gic_enable_irq(vi->vd.intid);

    /* Sürücü hazır */
    virtio_driver_ok(&vi->vd);
    vi->active = 1;

    const char *kind_str = vi->kind == VINPUT_KEYBOARD ? "KLAVYE" :
                           vi->kind == VINPUT_MOUSE    ? "FARE"   : "BİLİNMİYOR";
    kok("virtio-input: '%s' [%s] PCI %02x:%02x.%d INTID %u, %s transport, %d event buffer\n",
        vi->name, kind_str, pci->bus, pci->dev, pci->fn,
        vi->vd.intid, vi->vd.modern ? "modern" : "legacy", vi->nbufs);
    kinfo("virtio-input: features=0x%x, config @ %p\n", feat, (void *)vi->vd.cfgbase);

    ndev++;
    return 0;
}

void virtio_input_init(void)
{
    int found = 0;

    /* Legacy transitional (0x1012) veya modern (0x1052) ara */
    for (int i = 0; i < pci_device_count(); i++) {
        const struct pci_device *pci = pci_get_device(i);
        if (pci->vendor != PCI_VENDOR_VIRTIO)
            continue;
        if (pci->device == VIRTIO_INPUT_DEVICE_ID_LEGACY ||
            pci->device == VIRTIO_INPUT_DEVICE_ID_MODERN) {
            if (vinput_init_one(pci) == 0)
                found++;
        }
    }

    if (found == 0)
        kwarn("virtio-input aygıtı bulunamadı (QEMU'ya -device virtio-keyboard-pci ekleyin)\n");
    else
        kok("virtio-input: %d aygıt hazır\n", found);
}

/*
 * pci.c — PCI ECAM enumerasyonu (QEMU virt GPEX host bridge)
 */
#include <toros/pci.h>
#include <toros/printf.h>
#include <toros/string.h>

static struct pci_device devices[PCI_MAX_DEVICES];
static int ndevices;

/*
 * BAR adres atama (firmware yok — -kernel direct boot):
 * QEMU virt pencerelerine bump-allocation ile adres veriyoruz.
 */
static u64 mmio32_bump = PCI_MMIO32_BASE;      /* 0x10000000-0x3EFEFFFF */
static u64 mmio64_bump = PCI_MMIO64_BASE;      /* 0x8000000000+ */
static u32 io_bump;                            /* I/O port alanı (64KB) */

/* Dönüş: 1 = 64-bit BAR iki slot kapladı (sonrakini atla) */
static int pci_assign_bar(struct pci_device *d, int i)
{
    u64 base = d->ecam;
    u32 off = PCI_REG_BAR0 + i * 4;

    /*
     * Atanmamış BAR'lar reset sonrası 0 okunur; tip/maske ancak
     * 0xFFFFFFFF yazıldıktan sonra görünür. Bu yüzden önce boyutla.
     */
    mmio_write32(base + off, 0xFFFFFFFF);
    u32 v = mmio_read32(base + off);
    if (v == 0 || v == 0xFFFFFFFF) {
        mmio_write32(base + off, 0);
        return 0;
    }

    if (v & 1) {
        /* I/O BAR */
        u32 size = ~(v & ~3u) + 1;
        if (size == 0 || size > 0x10000) {
            mmio_write32(base + off, 0);
            return 0;
        }
        u32 addr = (u32)ALIGN_UP(io_bump, size);
        io_bump = addr + size;
        mmio_write32(base + off, addr);
        d->bar_addr[i] = PCI_IO_BASE + addr;
        d->bar_is_io[i] = 1;
    } else if (((v >> 1) & 3) == 2 && i < 5) {
        /* 64-bit memory BAR (iki slot kaplar) */
        mmio_write32(base + off + 4, 0xFFFFFFFF);
        u32 mask_hi = mmio_read32(base + off + 4);
        u64 mask = ((u64)mask_hi << 32) | (v & ~0xFu);
        u64 size = ~mask + 1;
        if (size == 0 || size > (1ULL << 32)) {
            mmio_write32(base + off, 0);
            mmio_write32(base + off + 4, 0);
            return 0;
        }
        u64 addr = ALIGN_UP(mmio64_bump, size);
        mmio64_bump = addr + size;
        mmio_write32(base + off, (u32)addr);
        mmio_write32(base + off + 4, (u32)(addr >> 32));
        d->bar_addr[i] = addr;
        return 1;   /* sonraki slot bu BAR'ın üst yarısı */
    } else {
        /* 32-bit memory BAR */
        u32 size = ~(v & ~0xFu) + 1;
        if (size == 0) {
            mmio_write32(base + off, 0);
            return 0;
        }
        u64 addr = ALIGN_UP(mmio32_bump, size);
        mmio32_bump = addr + size;
        mmio_write32(base + off, (u32)addr);
        d->bar_addr[i] = addr;
    }
    return 0;
}

static u64 ecam_addr(u8 bus, u8 dev, u8 fn, u32 off)
{
    return PCI_ECAM_BASE
         + ((u64)bus << 20)
         + ((u64)dev << 15)
         + ((u64)fn << 12)
         + off;
}

u32 pci_cfg_read32(const struct pci_device *d, u32 off)
{
    return mmio_read32(d->ecam + off);
}

u16 pci_cfg_read16(const struct pci_device *d, u32 off)
{
    return mmio_read16(d->ecam + off);
}

u8 pci_cfg_read8(const struct pci_device *d, u32 off)
{
    return mmio_read8(d->ecam + off);
}

void pci_cfg_write32(const struct pci_device *d, u32 off, u32 v)
{
    mmio_write32(d->ecam + off, v);
}

void pci_cfg_write16(const struct pci_device *d, u32 off, u16 v)
{
    mmio_write16(d->ecam + off, v);
}

void pci_cfg_write8(const struct pci_device *d, u32 off, u8 v)
{
    mmio_write8(d->ecam + off, v);
}

static int pci_probe_function(u8 bus, u8 dev, u8 fn)
{
    if (ndevices >= PCI_MAX_DEVICES)
        return 0;

    u64 base = ecam_addr(bus, dev, fn, 0);
    u16 vendor = mmio_read16(base + PCI_REG_VENDOR);
    if (vendor == 0xFFFF || vendor == 0x0000)
        return 0;

    struct pci_device *d = &devices[ndevices];
    memset(d, 0, sizeof(*d));
    d->bus = bus;
    d->dev = dev;
    d->fn = fn;
    d->ecam = base;
    d->vendor = vendor;
    d->device = mmio_read16(base + PCI_REG_DEVICE);
    d->class_code = mmio_read8(base + PCI_REG_CLASS);
    d->subclass = mmio_read8(base + PCI_REG_SUBCLASS);
    d->prog_if = mmio_read8(base + 0x09);
    d->revision = mmio_read8(base + 0x08);
    d->irq_pin = mmio_read8(base + PCI_REG_IRQPIN);
    d->irq_line = mmio_read8(base + PCI_REG_IRQLINE);

    /* BAR'lar */
    for (int i = 0; i < 6; i++) {
        u32 bar = mmio_read32(base + PCI_REG_BAR0 + i * 4);
        d->bar[i] = bar;
        d->bar_is_io[i] = bar & 1;
    }

    /* MSI-X capability? */
    u16 status = mmio_read16(base + PCI_REG_STATUS);
    if (status & BIT(4)) {
        u8 cap = mmio_read8(base + PCI_REG_CAPPTR);
        while (cap && cap != 0xFF) {
            u8 id = mmio_read8(base + cap);
            if (id == PCI_CAP_ID_MSIX) {
                d->has_msix = 1;
                break;
            }
            cap = mmio_read8(base + cap + 1);
        }
    }

    /*
     * IRQ line hesabı (QEMU GPEX swizzle: intx = (slot + pin0) % 4,
     * gpex irq base = SPI 3). Firmware olmadığı için line register'ını
     * biz programlıyoruz (bilgilendirme amaçlı konvansiyon).
     */
    if (d->irq_pin >= 1 && d->irq_pin <= 4) {
        u32 intx = (d->dev + (d->irq_pin - 1)) % 4;
        d->irq_line = (u8)(3 + intx);
        mmio_write8(base + PCI_REG_IRQLINE, d->irq_line);
    }

    /* BAR adres atama (firmware yerine) */
    for (int i = 0; i < 6; i++)
        i += pci_assign_bar(d, i);   /* 64-bit BAR ekstra slot atlar */

    /* Command: IO + MEM + BusMaster enable */
    u16 cmd = mmio_read16(base + PCI_REG_COMMAND);
    mmio_write16(base + PCI_REG_COMMAND, cmd | PCI_CMD_IO_EN | PCI_CMD_MEM_EN | PCI_CMD_BUSMSTR);

    kinfo("PCI %02x:%02x.%d  %04x:%04x  class %02x:%02x  irq line %d pin %d%s\n",
          d->bus, d->dev, d->fn, d->vendor, d->device,
          d->class_code, d->subclass, d->irq_line, d->irq_pin,
          d->has_msix ? "  [MSI-X]" : "");

    ndevices++;
    return 1;
}

void pci_init(void)
{
    ndevices = 0;
    for (u8 dev = 0; dev < 32; dev++) {
        u64 base = ecam_addr(0, dev, 0, 0);
        u16 vendor = mmio_read16(base + PCI_REG_VENDOR);
        if (vendor == 0xFFFF || vendor == 0x0000)
            continue;

        pci_probe_function(0, dev, 0);

        /* Multifunction? */
        u8 hdr = mmio_read8(base + PCI_REG_HEADERTYPE);
        if (hdr & 0x80) {
            for (u8 fn = 1; fn < 8; fn++)
                pci_probe_function(0, dev, fn);
        }
    }
    kok("PCI enumerasyon: %d aygıt\n", ndevices);
}

int pci_device_count(void)
{
    return ndevices;
}

const struct pci_device *pci_get_device(int idx)
{
    if (idx < 0 || idx >= ndevices)
        return NULL;
    return &devices[idx];
}

const struct pci_device *pci_find(u16 vendor, u16 device, int start_idx)
{
    for (int i = start_idx; i < ndevices; i++)
        if (devices[i].vendor == vendor && devices[i].device == device)
            return &devices[i];
    return NULL;
}

u64 pci_bar_address(const struct pci_device *d, int bar_idx)
{
    if (bar_idx < 0 || bar_idx >= 6)
        return 0;
    return d->bar_addr[bar_idx];
}

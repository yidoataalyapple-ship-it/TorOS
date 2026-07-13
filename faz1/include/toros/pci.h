/*
 * pci.h — PCI ECAM enumerasyonu (QEMU virt GPEX)
 */
#ifndef TOROS_PCI_H
#define TOROS_PCI_H

#include <toros/types.h>

/*
 * QEMU virt PCI bellek haritası.
 * highmem-ecam (default açık) ile aktif ECAM: 0x4010000000 (256 bus).
 */
#define PCI_ECAM_BASE   0x4010000000UL
#define PCI_ECAM_SIZE   0x10000000UL
#define PCI_MMIO32_BASE 0x10000000UL
#define PCI_IO_BASE     0x3EFF0000UL   /* I/O penceresi (legacy BAR0) */
#define PCI_MMIO64_BASE 0x8000000000UL

#define PCI_VENDOR_VIRTIO 0x1AF4

/* Config space offsetleri */
#define PCI_REG_VENDOR    0x00
#define PCI_REG_DEVICE    0x02
#define PCI_REG_COMMAND   0x04
#define PCI_REG_STATUS    0x06
#define PCI_REG_CLASS     0x0B
#define PCI_REG_SUBCLASS  0x0A
#define PCI_REG_HEADERTYPE 0x0E
#define PCI_REG_BAR0      0x10
#define PCI_REG_CAPPTR    0x34
#define PCI_REG_IRQLINE   0x3C
#define PCI_REG_IRQPIN    0x3D

#define PCI_CMD_IO_EN   BIT(0)
#define PCI_CMD_MEM_EN  BIT(1)
#define PCI_CMD_BUSMSTR BIT(2)

#define PCI_CAP_ID_MSIX 0x11

#define PCI_MAX_DEVICES 64

struct pci_device {
    u8  bus, dev, fn;
    u16 vendor, device;
    u8  class_code, subclass, prog_if, revision;
    u8  irq_pin, irq_line;
    u32 bar[6];            /* ham BAR değerleri (reset sonrası) */
    u64 bar_addr[6];       /* atanmış efektif CPU adresi (0=yok) */
    int bar_is_io[6];
    int has_msix;
    u64 ecam;              /* config space taban adresi */
};

u32 pci_cfg_read32(const struct pci_device *d, u32 off);
u16 pci_cfg_read16(const struct pci_device *d, u32 off);
u8  pci_cfg_read8(const struct pci_device *d, u32 off);
void pci_cfg_write32(const struct pci_device *d, u32 off, u32 v);
void pci_cfg_write16(const struct pci_device *d, u32 off, u16 v);
void pci_cfg_write8(const struct pci_device *d, u32 off, u8 v);

void pci_init(void);
int  pci_device_count(void);
const struct pci_device *pci_get_device(int idx);
const struct pci_device *pci_find(u16 vendor, u16 device, int start_idx);

/* BAR efektif CPU adresi (I/O ise pencere tabanı eklenir) */
u64 pci_bar_address(const struct pci_device *d, int bar_idx);

#endif

/*
 * mmu.c — ARMv8-A 4 seviyeli MMU (4KB granül, 48-bit VA)
 *
 * Identity map stratejisi (1GB blok descriptor'ları):
 *   0x0000_0000 - 0x3FFF_FFFF : DEVICE  (GIC, UART, PCI MMIO32, PCI I/O)
 *   0x4000_0000 - 0xFFFF_FFFF : NORMAL  (2GB+ RAM + kernel)
 *   0x40_1000_0000 bölgesi    : DEVICE  (PCI ECAM)
 *   0x80_0000_0000 bölgesi    : DEVICE  (PCI MMIO64)
 */
#include <toros/mmu.h>
#include <toros/printf.h>
#include <toros/string.h>

/* Descriptor tipleri */
#define DESC_TABLE 0x3UL
#define DESC_BLOCK 0x1UL

/* MAIR indeksleri */
#define MT_NORMAL 0
#define MT_DEVICE 1

#define MAIR_VALUE ((0xFFUL << (MT_NORMAL * 8)) | (0x04UL << (MT_DEVICE * 8)))

/* Block descriptor alanları */
#define BD_ADDR_MASK 0x0000FFFFC0000000UL   /* 1GB blok: [47:30] */
#define BD_ATTRIDX(x) ((u64)(x) << 2)
#define BD_AP_RW_EL1  (0UL << 6)
#define BD_SH_INNER   (3UL << 8)
#define BD_AF         (1UL << 10)
#define BD_PXN        (1UL << 53)
#define BD_UXN        (1UL << 54)

#define BLOCK_NORMAL(addr) \
    (DESC_BLOCK | BD_ATTRIDX(MT_NORMAL) | BD_AP_RW_EL1 | BD_SH_INNER | BD_AF | \
     ((u64)(addr) & BD_ADDR_MASK))

#define BLOCK_DEVICE(addr) \
    (DESC_BLOCK | BD_ATTRIDX(MT_DEVICE) | BD_AP_RW_EL1 | BD_AF | BD_PXN | BD_UXN | \
     ((u64)(addr) & BD_ADDR_MASK))

/* 4 seviye: L0(512G) -> L1(1G) -> L2(2M) -> L3(4K) */
#define TABLE_ENTRIES 512

static u64 l0_table[TABLE_ENTRIES] __attribute__((aligned(4096)));
static u64 l1_low[TABLE_ENTRIES] __attribute__((aligned(4096)));   /* L0[0]   */
static u64 l1_ecam[TABLE_ENTRIES] __attribute__((aligned(4096)));  /* L0[32]  */
static u64 l1_hi[TABLE_ENTRIES] __attribute__((aligned(4096)));    /* L0[64]  */

static void build_tables(void)
{
    memset(l0_table, 0, sizeof(l0_table));
    memset(l1_low, 0, sizeof(l1_low));
    memset(l1_ecam, 0, sizeof(l1_ecam));
    memset(l1_hi, 0, sizeof(l1_hi));

    /*
     * L0 girişi = 512GB (2^39), L1 girişi = 1GB (2^30).
     * L0[0] -> l1_low: 0 - 512GB arası tüm ihtiyaçlar burada.
     */
    l0_table[0] = ((u64)l1_low & 0x0000FFFFFFFFF000UL) | DESC_TABLE;

    /* 0x00000000-0x3FFFFFFF: device (GIC/UART/PCI MMIO32 + I/O penceresi) */
    l1_low[0] = BLOCK_DEVICE(0x00000000UL);
    /* 0x40000000-0xBFFFFFFF: RAM normal (2GB+, kernel burada) */
    l1_low[1] = BLOCK_NORMAL(0x40000000UL);
    l1_low[2] = BLOCK_NORMAL(0x80000000UL);
    /* 0xC0000000-0xFFFFFFFF: RAM genişleme / MMIO32 üst kısım */
    l1_low[3] = BLOCK_NORMAL(0xC0000000UL);
    /* 0x4000000000-0x403FFFFFFF: PCI ECAM (0x4010000000) — L1 index = 256 */
    l1_low[256] = BLOCK_DEVICE(0x4000000000UL);

    /* L0[1] -> l1_hi: 0x8000000000 bölgesi (PCI MMIO64 penceresi) */
    l0_table[1] = ((u64)l1_hi & 0x0000FFFFFFFFF000UL) | DESC_TABLE;
    l1_hi[0] = BLOCK_DEVICE(0x8000000000UL);

    (void)l1_ecam;   /* ileride ayrıntılı mapping için rezerve */
}

void mmu_init(void)
{
    build_tables();

    /* MAIR */
    __asm__ volatile("msr mair_el1, %0" :: "r"(MAIR_VALUE));

    /*
     * TCR_EL1:
     *  T0SZ=16 (48-bit VA), TG0=4KB, IRGN0/ORGN0=WBWA,
     *  SH0=inner, EPD1=1 (TTBR1 kapalı), IPS=40-bit (1TB)
     */
    u64 tcr = (16UL)             /* T0SZ */
            | (1UL << 8)         /* IRGN0 = WBWA */
            | (1UL << 10)        /* ORGN0 = WBWA */
            | (3UL << 12)        /* SH0 = inner shareable */
            | (0UL << 14)        /* TG0 = 4KB */
            | (1UL << 23)        /* EPD1 */
            | (2UL << 32);       /* IPS = 40 bits */
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));

    /* TTBR0 */
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((u64)l0_table));

    /* TLB temizle */
    __asm__ volatile("tlbi vmalle1");
    dsb_ish();
    isb();

    /* SCTLR_EL1: RES1 bitleri + M + C + I */
    u64 sctlr = 0x30D00800UL | BIT(0) | BIT(2) | BIT(12);
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    isb();

    kok("MMU aktif: 4 seviye, 48-bit VA, identity map (RAM normal / MMIO device)\n");
}

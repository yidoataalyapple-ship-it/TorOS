/*
 * gic.h — GICv3 sürücüsü (QEMU virt)
 * (Planda "MEVCUT" sayılan temel bileşen)
 */
#ifndef TOROS_GIC_H
#define TOROS_GIC_H

#include <toros/types.h>

/* QEMU virt GICv3 adresleri */
#define GICD_BASE 0x08000000UL
#define GICR_BASE 0x080A0000UL
#define GICR_STRIDE 0x20000UL   /* RD(64K) + SGI(64K) per CPU */

/* GIC Distributor (GICD) */
#define GICD_CTLR        0x000
#define GICD_TYPER       0x004
#define GICD_IIDR        0x008
#define GICD_IGROUPR(n)  (0x080 + 4 * (n))
#define GICD_ISENABLER(n) (0x100 + 4 * (n))
#define GICD_ICENABLER(n) (0x180 + 4 * (n))
#define GICD_ISPENDR(n)  (0x200 + 4 * (n))
#define GICD_ICPENDR(n)  (0x280 + 4 * (n))
#define GICD_IPRIORITYR(n) (0x400 + 4 * (n))
#define GICD_ICFGR(n)    (0xC00 + 4 * (n))
#define GICD_IROUTER(n)  (0x6100 + 8 * (n))

#define GICD_CTLR_EN_GRP0    BIT(0)
#define GICD_CTLR_EN_GRP1_NS BIT(1)
#define GICD_CTLR_ARE_NS     BIT(4)
#define GICD_CTLR_RWP        BIT(31)

/* GIC Redistributor (GICR) — SGI/PPI sayfası RD+0x10000 */
#define GICR_WAKER       0x014
#define GICR_WAKER_PSLEEP BIT(1)
#define GICR_WAKER_CASLEEP BIT(2)
#define GICR_IGROUPR0    0x10080
#define GICR_ISENABLER0  0x10100
#define GICR_ICENABLER0  0x10180
#define GICR_IPRIORITYR(n) (0x10400 + 4 * (n))
#define GICR_ICFGR(n)    (0x10C00 + 4 * (n))

/* Sistem register erişimleri (ICC) */
static inline void gic_write_sre(u64 v)
{
    __asm__ volatile("msr icc_sre_el1, %0" :: "r"(v));
}

static inline u64 gic_read_sre(void)
{
    u64 v;
    __asm__ volatile("mrs %0, icc_sre_el1" : "=r"(v));
    return v;
}

static inline void gic_set_pmr(u64 v)
{
    __asm__ volatile("msr icc_pmr_el1, %0" :: "r"(v));
}

static inline void gic_enable_grp1(void)
{
    __asm__ volatile("msr icc_igrpen1_el1, %0" :: "r"(1UL));
}

static inline u64 gic_read_iar1(void)
{
    u64 v;
    __asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(v));
    return v;
}

static inline void gic_write_eoir1(u64 v)
{
    __asm__ volatile("msr icc_eoir1_el1, %0" :: "r"(v));
}

void gic_init(void);
void gic_enable_irq(u32 intid);
void gic_disable_irq(u32 intid);
void gic_set_priority(u32 intid, u8 prio);
void gic_set_edge_triggered(u32 intid, int edge);

#endif

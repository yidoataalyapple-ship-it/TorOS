/*
 * gic.c — GICv3 sürücüsü (system register arayüzü, QEMU virt)
 */
#include <toros/gic.h>
#include <toros/printf.h>

#define MAX_SPI_REGS 32   /* 1024 interrupt'a kadar grup/enable register'ı */

static u64 gicr_base(void)
{
    /* CPU0 redistributor (SMP: Aff0 ile indeksle) */
    u64 aff0 = read_mpidr() & 0xff;
    return GICR_BASE + aff0 * GICR_STRIDE;
}

static void gicd_write(u32 off, u32 v) { mmio_write32(GICD_BASE + off, v); }
static u32  gicd_read(u32 off)         { return mmio_read32(GICD_BASE + off); }

static void gicr_write(u32 off, u32 v) { mmio_write32(gicr_base() + off, v); }
static u32  gicr_read(u32 off)         { return mmio_read32(gicr_base() + off); }

static void gicd_wait_rwp(void)
{
    while (gicd_read(GICD_CTLR) & GICD_CTLR_RWP)
        ;
}

void gic_init(void)
{
    u32 typer = gicd_read(GICD_TYPER);
    u32 itlines = typer & 0x1f;

    /* 1) Distributor'ı kapat, yapılandır */
    gicd_write(GICD_CTLR, 0);
    gicd_wait_rwp();

    /* Tüm SPI'lar: Group 1 NS, disable, düşük öncelik, level */
    for (u32 i = 0; i < MAX_SPI_REGS && i <= itlines; i++) {
        gicd_write(GICD_ICENABLER(i), 0xFFFFFFFF);
        gicd_write(GICD_IGROUPR(i), 0xFFFFFFFF);
        gicd_write(GICD_IPRIORITYR(i), 0xA0A0A0A0);
        gicd_write(GICD_ICFGR(i), 0);
    }

    /* SPI yönlendirme: CPU0 (affinity 0.0.0.0) */
    for (u32 id = 32; id < 1020; id++) {
        if (id / 32 > itlines)
            break;
        mmio_write64(GICD_BASE + GICD_IROUTER(id), 0);
    }

    /* ARE_NS + Group 1 NS enable */
    gicd_write(GICD_CTLR, GICD_CTLR_ARE_NS | GICD_CTLR_EN_GRP1_NS);
    gicd_wait_rwp();

    /* 2) Redistributor uyandır */
    gicr_write(GICR_WAKER, 0);
    while (gicr_read(GICR_WAKER) & GICR_WAKER_CASLEEP)
        ;

    /* 3) SGI/PPI (0-31): Group 1, disable, öncelik */
    gicr_write(GICR_ICENABLER0, 0xFFFFFFFF);
    gicr_write(GICR_IGROUPR0, 0xFFFFFFFF);
    for (u32 i = 0; i < 8; i++)
        gicr_write(GICR_IPRIORITYR(i), 0xA0A0A0A0);

    /* 4) CPU arayüzü (ICC) */
    u64 sre = gic_read_sre();
    sre |= BIT(0);          /* SRE: system register erişimi */
    sre &= ~BIT(3);         /* DIB = 0 */
    sre &= ~BIT(2);         /* DFB = 0 */
    gic_write_sre(sre);
    isb();

    gic_set_pmr(0xFF);      /* tüm öncelikler geçer */
    gic_enable_grp1();
    isb();

    kok("GICv3 aktif: GICD=%p GICR=%p, %u interrupt hattı\n",
        (void *)GICD_BASE, (void *)gicr_base(), (itlines + 1) * 32);
}

void gic_enable_irq(u32 intid)
{
    if (intid < 32) {
        gicr_write(GICR_ISENABLER0, BIT(intid));
    } else if (intid < 1020) {
        gicd_write(GICD_ISENABLER(intid / 32), BIT(intid % 32));
    }
}

void gic_disable_irq(u32 intid)
{
    if (intid < 32) {
        gicr_write(GICR_ICENABLER0, BIT(intid));
    } else if (intid < 1020) {
        gicd_write(GICD_ICENABLER(intid / 32), BIT(intid % 32));
    }
}

void gic_set_priority(u32 intid, u8 prio)
{
    u32 reg_off = (intid / 4) * 4;
    u32 shift = (intid % 4) * 8;

    if (intid < 32) {
        u32 v = gicr_read(GICR_IPRIORITYR(intid / 4));
        v &= ~(0xFFu << shift);
        v |= ((u32)prio << shift);
        gicr_write(GICR_IPRIORITYR(intid / 4), v);
    } else {
        u32 v = gicd_read(GICD_IPRIORITYR(intid / 4));
        (void)reg_off;
        v &= ~(0xFFu << shift);
        v |= ((u32)prio << shift);
        gicd_write(GICD_IPRIORITYR(intid / 4), v);
    }
}

void gic_set_edge_triggered(u32 intid, int edge)
{
    if (intid < 32) {
        u32 n = intid / 16;
        u32 shift = (intid % 16) * 2 + 1;
        u32 v = gicr_read(GICR_ICFGR(n));
        if (edge) v |= BIT(shift); else v &= ~BIT(shift);
        gicr_write(GICR_ICFGR(n), v);
    } else {
        u32 n = intid / 16;
        u32 shift = (intid % 16) * 2 + 1;
        u32 v = gicd_read(GICD_ICFGR(n));
        if (edge) v |= BIT(shift); else v &= ~BIT(shift);
        gicd_write(GICD_ICFGR(n), v);
    }
}

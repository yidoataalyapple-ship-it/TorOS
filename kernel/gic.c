/*
 * torOS GICv3 Driver
 * Generic Interrupt Controller v3
 * QEMU virt machine
 * 
 * GICD: Distributor @ 0x08000000
 * GICR: Redistributor @ 0x080A0000
 */

#include "../include/toros.h"

/* GICD registers */
#define GICD_BASE       0x08000000
#define GICD_CTLR       0x0000      /* Distributor Control */
#define GICD_TYPER      0x0004      /* Interrupt Controller Type */
#define GICD_IIDR       0x0008      /* Distributor Implementer ID */
#define GICD_IGROUPR    0x0080      /* Interrupt Group Registers */
#define GICD_ISENABLER  0x0100      /* Interrupt Set-Enable */
#define GICD_ICENABLER  0x0180      /* Interrupt Clear-Enable */
#define GICD_ISPENDR    0x0200      /* Interrupt Set-Pending */
#define GICD_ICPENDR    0x0280      /* Interrupt Clear-Pending */
#define GICD_ISACTIVER  0x0300      /* Interrupt Set-Active */
#define GICD_IPRIORITYR 0x0400      /* Interrupt Priority */
#define GICD_ITARGETSR  0x0800      /* Interrupt Processor Targets */
#define GICD_ICFGR      0x0C00      /* Interrupt Configuration */

/* GICR registers (per-CPU redistributor) */
#define GICR_BASE       0x080A0000
#define GICR_CTLR       0x0000
#define GICR_IIDR       0x0004
#define GICR_TYPER      0x0008
#define GICR_STATUSR    0x0010
#define GICR_WAKER      0x0014

/* SGI/PPI frame (inside redistributor) */
#define GICR_SGI_OFFSET 0x10000
#define GICR_IGROUPR0   0x10080
#define GICR_ISENABLER0 0x10100
#define GICR_ICENABLER0 0x10180
#define GICR_ISPENDR0   0x10200
#define GICR_ICPENDR0   0x10280
#define GICR_ISACTIVER0 0x10300
#define GICR_IPRIORITYR 0x10400
#define GICR_ICFGR0     0x10C00

/* GICC (CPU interface) via system registers */
#define ICC_IGRPEN1_EL1     "S3_0_C12_C12_7"
#define ICC_PMR_EL1         "S3_0_C4_C6_0"
#define ICC_BPR1_EL1        "S3_0_C12_C12_3"
#define ICC_CTLR_EL1        "S3_0_C12_C12_4"
#define ICC_SRE_EL1         "S3_0_C12_C12_5"
#define ICC_SRE_EL2         "S3_4_C12_C9_5"
#define ICC_IAR1_EL1        "S3_0_C12_C12_0"
#define ICC_EOIR1_EL1       "S3_0_C12_C12_1"

static volatile uint32 *gicd = (volatile uint32 *)GICD_BASE;
static volatile uint32 *gicr = (volatile uint32 *)GICR_BASE;

static inline uint32 gicd_read(uint reg)
{
    return gicd[reg >> 2];
}

static inline void gicd_write(uint reg, uint32 val)
{
    gicd[reg >> 2] = val;
}

static inline uint32 gicr_read(uint reg)
{
    return gicr[reg >> 2];
}

static inline void gicr_write(uint reg, uint32 val)
{
    gicr[reg >> 2] = val;
}

/* Enable system register access to GICC */
static void gicc_enable_sre(void)
{
    uint64 val;
    
    /* Enable at EL2 if present */
    __asm__ volatile("mrs %0, " ICC_SRE_EL2 : "=r"(val));
    val |= (1 << 0);  /* SRE bit */
    val |= (1 << 3);  /* Enable EL1 access */
    __asm__ volatile("msr " ICC_SRE_EL2 ", %0" :: "r"(val));
    
    /* Enable at EL1 */
    __asm__ volatile("mrs %0, " ICC_SRE_EL1 : "=r"(val));
    val |= (1 << 0);  /* SRE bit */
    __asm__ volatile("msr " ICC_SRE_EL1 ", %0" :: "r"(val));
    
    isb();
}

/* GICD init */
static void gicd_init(void)
{
    /* Disable distributor */
    gicd_write(GICD_CTLR, 0);
    
    /* Wait for disabled state */
    while (gicd_read(GICD_CTLR) & (1 << 31))
        ;
    
    /* Get number of SPIs */
    uint32 typer = gicd_read(GICD_TYPER);
    uint32 num_spis = ((typer >> 5) & 0x1F) + 1;
    num_spis *= 32;  /* 32 interrupts per block */
    
    /* Set all SPIs to group 1 (non-secure) */
    for (uint i = 0; i < num_spis / 32; i++)
        gicd_write(GICD_IGROUPR + i * 4, 0xFFFFFFFF);
    
    /* Set all SPIs to lowest priority */
    for (uint i = 8; i < num_spis; i += 4)
        gicd_write(GICD_IPRIORITYR + i, 0xA0A0A0A0);
    
    /* Disable all SPIs */
    for (uint i = 0; i < num_spis / 32; i++)
        gicd_write(GICD_ICENABLER + i * 4, 0xFFFFFFFF);
    
    /* Enable distributor */
    gicd_write(GICD_CTLR, 0x13);  /* EnableGrp0 | EnableGrp1NS | ARE_NS */
}

/* GICR init (per-CPU) */
static void gicr_init(void)
{
    /* Wake up redistributor */
    uint32 waker = gicr_read(GICR_WAKER);
    waker &= ~(1 << 1);  /* Clear ProcessorSleep */
    gicr_write(GICR_WAKER, waker);
    
    /* Wait until ChildrenAsleep is clear */
    while (gicr_read(GICR_WAKER) & (1 << 3))
        ;
    
    /* Set all SGIs/PPIs to group 1 */
    gicr_write(GICR_IGROUPR0, 0xFFFFFFFF);
    
    /* Set all SGIs/PPIs to lowest priority */
    for (uint i = 0; i < 32; i += 4)
        gicr_write(GICR_IPRIORITYR + i, 0xA0A0A0A0);
    
    /* Disable all SGIs/PPIs */
    gicr_write(GICR_ICENABLER0, 0xFFFFFFFF);
    
    /* Configure SGIs/PPIs as level-sensitive (except SGIs) */
    gicr_write(GICR_ICFGR0, 0);      /* SGIs */
    gicr_write(GICR_ICFGR0 + 4, 0);  /* PPIs */
}

/* GICC init (CPU interface) */
static void gicc_init(void)
{
    /* Set priority mask (lowest priority that can preempt) */
    __asm__ volatile("msr " ICC_PMR_EL1 ", %0" :: "r"(0xFF));
    
    /* Set binary point: no preemption within subgroup */
    __asm__ volatile("msr " ICC_BPR1_EL1 ", %0" :: "r"(0));
    
    /* Enable group 1 interrupts */
    __asm__ volatile("msr " ICC_IGRPEN1_EL1 ", %0" :: "r"(1));
    
    isb();
}

void gic_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Initializing GICv3...\n");
    
    /* Enable system register access */
    gicc_enable_sre();
    
    /* Init all GIC components */
    gicd_init();
    gicr_init();
    gicc_init();
    
    printk_color(TERM_GREEN, "[BOOT] GICv3 initialized\n");
}

/* Enable an interrupt */
void gic_enable_irq(uint irq)
{
    if (irq < 32) {
        /* SGI/PPI */
        gicr_write(GICR_ISENABLER0, (1U << irq));
    } else {
        /* SPI */
        uint reg = GICD_ISENABLER + ((irq / 32) * 4);
        gicd_write(reg, (1U << (irq % 32)));
    }
}

/* Disable an interrupt */
void gic_disable_irq(uint irq)
{
    if (irq < 32) {
        gicr_write(GICR_ICENABLER0, (1U << irq));
    } else {
        uint reg = GICD_ICENABLER + ((irq / 32) * 4);
        gicd_write(reg, (1U << (irq % 32)));
    }
}

/* Set interrupt priority */
void gic_set_priority(uint irq, uint8 prio)
{
    if (irq < 32) {
        uint reg = GICR_IPRIORITYR + ((irq / 4) * 4);
        uint32 val = gicr_read(reg);
        val &= ~(0xFF << ((irq % 4) * 8));
        val |= ((uint32)prio << ((irq % 4) * 8));
        gicr_write(reg, val);
    } else {
        uint reg = GICD_IPRIORITYR + irq;
        /* Byte access */
        volatile uint8 *ptr = (volatile uint8 *)GICD_BASE + reg;
        *ptr = prio;
    }
}

/* Set interrupt target CPU */
void gic_set_target(uint irq, uint cpu)
{
    if (irq < 32) return;  /* SGIs/PPI are per-CPU */
    
    uint reg = GICD_ITARGETSR + irq;
    volatile uint8 *ptr = (volatile uint8 *)GICD_BASE + reg;
    *ptr = (1U << cpu);
}

/* Get pending interrupt number */
uint gic_get_irq(void)
{
    uint64 iar;
    __asm__ volatile("mrs %0, " ICC_IAR1_EL1 : "=r"(iar));
    return (uint)(iar & 0xFFFFFF);
}

/* Signal end of interrupt handling */
void gic_eoi(uint irq)
{
    __asm__ volatile("msr " ICC_EOIR1_EL1 ", %0" :: "r"((uint64)irq));
}

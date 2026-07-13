/*
 * torOS Virtual Memory Manager
 * ARM64 MMU with 4-level page tables (4KB granule)
 * 
 * Uses 48-bit virtual addresses with 4KB pages
 * Level 0: PGD (Page Global Directory)
 * Level 1: PUD (Page Upper Directory)
 * Level 2: PMD (Page Middle Directory)
 * Level 3: PTE (Page Table Entry)
 */

#include "../include/toros.h"

/* ARM64 MMU configuration */
#define VA_BITS         48
#define PGD_SHIFT       39       /* Level 0 */
#define PUD_SHIFT       30       /* Level 1 */
#define PMD_SHIFT       21       /* Level 2 */
#define PTE_SHIFT       12       /* Level 3 */

#define ENTRIES_PER_TABLE   512  /* 4KB page / 8 bytes per entry */

/* Page table entry flags */
#define PTE_VALID       (1 << 0)     /* Valid entry */
#define PTE_TABLE       (1 << 1)     /* Points to next table (not block) */
#define PTE_PAGE        (1 << 1)     /* Page descriptor */
#define PTE_AF          (1 << 10)    /* Access flag */
#define PTE_SH_INNER    (3 << 8)     /* Inner shareable */
#define PTE_AP_RW       (0 << 6)     /* Read-write at EL1 */
#define PTE_AP_RO       (2 << 6)     /* Read-only */
#define PTE_UXN         (1 << 54)    /* Unprivileged execute-never */
#define PTE_PXN         (1 << 53)    /* Privileged execute-never */
#define PTE_ATTR_IDX(n) ((n) << 2)   /* MAIR attribute index */

#define PTE_KERNEL      (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_AP_RW | PTE_PXN | PTE_ATTR_IDX(0))
#define PTE_DEVICE      (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_AP_RW | PTE_PXN | PTE_UXN | PTE_ATTR_IDX(1))
#define PTE_NORMAL      (PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_AP_RW | PTE_PXN | PTE_ATTR_IDX(0))

/* MAIR attributes */
#define MAIR_ATTR(n, attr)  ((attr) << ((n) * 8))
#define MAIR_DEVICE_nGnRnE  0x00    /* Device, non-Gathering, non-Reordering, no Early-write */
#define MAIR_NORMAL_WB      0xFF    /* Normal, Write-Back, Read/Write-Allocate */

/* TCR (Translation Control Register) */
#define TCR_T0SZ_48BIT      (64 - 48)       /* 48-bit VA space */
#define TCR_IRGN0_WBWA      (1 << 8)        /* Inner cacheability */
#define TCR_ORGN0_WBWA      (1 << 10)       /* Outer cacheability */
#define TCR_SH0_INNER       (3 << 12)       /* Inner shareable */
#define TCR_TG0_4KB         (0 << 14)       /* 4KB granule */
#define TCR_PS_256TB        (5 << 32)       /* 48-bit PA */

static uint64 *pgd;     /* Page Global Directory */
static uint64 *early_pgd;

/* MAIR setup value */
static inline uint64 mair_value(void)
{
    return MAIR_ATTR(0, MAIR_NORMAL_WB) |
           MAIR_ATTR(1, MAIR_DEVICE_nGnRnE);
}

/* TCR setup value */
static inline uint64 tcr_value(void)
{
    return TCR_T0SZ_48BIT |
           TCR_IRGN0_WBWA |
           TCR_ORGN0_WBWA |
           TCR_SH0_INNER |
           TCR_TG0_4KB |
           TCR_PS_256TB;
}

/* Get index at a specific level */
static inline uint pgd_idx(uint64 va) { return (va >> PGD_SHIFT) & 0x1FF; }
static inline uint pud_idx(uint64 va) { return (va >> PUD_SHIFT) & 0x1FF; }
static inline uint pmd_idx(uint64 va) { return (va >> PMD_SHIFT) & 0x1FF; }
static inline uint pte_idx(uint64 va) { return (va >> PTE_SHIFT) & 0x1FF; }

/* Allocate a page table */
static uint64 *alloc_table(void)
{
    uint64 *table = (uint64 *)page_alloc();
    if (!table) return NULL;
    memset(table, 0, PAGE_SIZE);
    return table;
}

/* Create a table entry pointing to next level */
static inline uint64 make_table_entry(uint64 *next_table)
{
    return ((uint64)next_table & 0xFFFFFFFFF000ULL) | PTE_VALID | PTE_TABLE;
}

/* Create a block/page entry */
static inline uint64 make_page_entry(uint64 pa, uint64 flags)
{
    return (pa & 0xFFFFFFFFF000ULL) | flags;
}

/* 
 * Identity map a region
 * pa = va (1:1 mapping for kernel)
 */
static int map_region(uint64 pa, uint64 size, uint64 flags)
{
    uint64 va = pa;  /* Identity mapping */
    uint64 end = va + size;

    for (; va < end; va += PAGE_SIZE, pa += PAGE_SIZE) {
        uint i0 = pgd_idx(va);
        uint i1 = pud_idx(va);
        uint i2 = pmd_idx(va);
        uint i3 = pte_idx(va);

        /* Level 0 - PGD */
        if (!(pgd[i0] & PTE_VALID)) {
            uint64 *pud = alloc_table();
            if (!pud) return -1;
            pgd[i0] = make_table_entry(pud);
        }
        uint64 *pud = (uint64 *)(pgd[i0] & 0xFFFFFFFFF000ULL);

        /* Level 1 - PUD */
        if (!(pud[i1] & PTE_VALID)) {
            uint64 *pmd = alloc_table();
            if (!pmd) return -1;
            pud[i1] = make_table_entry(pmd);
        }
        uint64 *pmd = (uint64 *)(pud[i1] & 0xFFFFFFFFF000ULL);

        /* Level 2 - PMD */
        if (!(pmd[i2] & PTE_VALID)) {
            uint64 *pte = alloc_table();
            if (!pte) return -1;
            pmd[i2] = make_table_entry(pte);
        }
        uint64 *pte = (uint64 *)(pmd[i2] & 0xFFFFFFFFF000ULL);

        /* Level 3 - PTE */
        pte[i3] = make_page_entry(pa, flags);
    }
    return 0;
}

void vm_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Initializing virtual memory (MMU)...\n");

    /* Allocate PGD */
    pgd = alloc_table();
    if (!pgd) {
        panic("Failed to allocate PGD");
    }

    /* 
     * Identity map kernel region (0x40000000 - kernel_end)
     * Normal, cacheable memory for code/data
     */
    uint64 kernel_size = (uint64)_kernel_end - KERNEL_BASE;
    kernel_size = (kernel_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    if (map_region(KERNEL_BASE, kernel_size, PTE_KERNEL) < 0)
        panic("Failed to map kernel");

    /* Map heap and free memory region */
    uint64 heap_start = (uint64)_kernel_end;
    heap_start = (heap_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64 remaining = RAM_SIZE - (heap_start - KERNEL_BASE);
    
    if (map_region(heap_start, remaining, PTE_NORMAL) < 0)
        panic("Failed to map heap");

    /* Map UART device region */
    if (map_region(0x09000000, PAGE_SIZE, PTE_DEVICE) < 0)
        panic("Failed to map UART");

    /* Map GIC regions */
    if (map_region(0x08000000, 0x10000, PTE_DEVICE) < 0)
        printk_color(TERM_RED, "[VM] Warning: Failed to map GIC\n");

    /* Setup MAIR */
    __asm__ volatile("msr MAIR_EL1, %0" :: "r"(mair_value()));

    /* Setup TCR */
    __asm__ volatile("msr TCR_EL1, %0" :: "r"(tcr_value()));
    isb();

    /* Load PGD into TTBR0_EL1 */
    uint64 pgd_pa = (uint64)pgd;
    __asm__ volatile("msr TTBR0_EL1, %0" :: "r"(pgd_pa));
    isb();

    /* Enable MMU */
    uint64 sctlr;
    __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
    sctlr |= (1 << 0);   /* M: Enable MMU */
    sctlr |= (1 << 2);   /* C: Enable data cache */
    sctlr |= (1 << 12);  /* I: Enable instruction cache */
    __asm__ volatile("msr SCTLR_EL1, %0" :: "r"(sctlr));
    isb();

    printk_color(TERM_GREEN, "[BOOT] MMU enabled - Virtual memory active\n");
    printk_color(TERM_GREEN, "[VM] Kernel mapped: %p - %p\n", 
                 KERNEL_BASE, KERNEL_BASE + kernel_size);
}

/* 
 * Map a single page for user space
 * Returns 0 on success, -1 on failure
 */
int vm_user_map(uint64 user_va, uint64 pa, uint64 flags)
{
    uint i0 = pgd_idx(user_va);
    uint i1 = pud_idx(user_va);
    uint i2 = pmd_idx(user_va);
    uint i3 = pte_idx(user_va);

    if (!(pgd[i0] & PTE_VALID)) {
        uint64 *pud = alloc_table();
        if (!pud) return -1;
        pgd[i0] = make_table_entry(pud);
    }
    uint64 *pud = (uint64 *)(pgd[i0] & 0xFFFFFFFFF000ULL);

    if (!(pud[i1] & PTE_VALID)) {
        uint64 *pmd = alloc_table();
        if (!pmd) return -1;
        pud[i1] = make_table_entry(pmd);
    }
    uint64 *pmd = (uint64 *)(pud[i1] & 0xFFFFFFFFF000ULL);

    if (!(pmd[i2] & PTE_VALID)) {
        uint64 *pte = alloc_table();
        if (!pte) return -1;
        pmd[i2] = make_table_entry(pte);
    }
    uint64 *pte = (uint64 *)(pmd[i2] & 0xFFFFFFFFF000ULL);

    pte[i3] = make_page_entry(pa, flags);
    
    /* TLB invalidate for this page */
    __asm__ volatile("tlbi VAAE1, %0" :: "r"(user_va >> 12));
    dsb();
    isb();
    
    return 0;
}

/* 
 * Unmap a user page
 */
void vm_user_unmap(uint64 user_va)
{
    uint i0 = pgd_idx(user_va);
    uint i1 = pud_idx(user_va);
    uint i2 = pmd_idx(user_va);
    uint i3 = pte_idx(user_va);

    if (!(pgd[i0] & PTE_VALID)) return;
    uint64 *pud = (uint64 *)(pgd[i0] & 0xFFFFFFFFF000ULL);

    if (!(pud[i1] & PTE_VALID)) return;
    uint64 *pmd = (uint64 *)(pud[i1] & 0xFFFFFFFFF000ULL);

    if (!(pmd[i2] & PTE_VALID)) return;
    uint64 *pte = (uint64 *)(pmd[i2] & 0xFFFFFFFFF000ULL);

    pte[i3] = 0;
    
    /* TLB invalidate */
    __asm__ volatile("tlbi VAAE1, %0" :: "r"(user_va >> 12));
    dsb();
    isb();
}

/* 
 * Get physical address for a virtual address
 */
uint64 vm_va2pa(uint64 va)
{
    uint i0 = pgd_idx(va);
    uint i1 = pud_idx(va);
    uint i2 = pmd_idx(va);
    uint i3 = pte_idx(va);

    if (!(pgd[i0] & PTE_VALID)) return 0;
    uint64 *pud = (uint64 *)(pgd[i0] & 0xFFFFFFFFF000ULL);

    if (!(pud[i1] & PTE_VALID)) return 0;
    uint64 *pmd = (uint64 *)(pud[i1] & 0xFFFFFFFFF000ULL);

    if (!(pmd[i2] & PTE_VALID)) return 0;
    uint64 *pte = (uint64 *)(pmd[i2] & 0xFFFFFFFFF000ULL);

    if (!(pte[i3] & PTE_VALID)) return 0;
    
    return (pte[i3] & 0xFFFFFFFFF000ULL) | (va & 0xFFF);
}

/* Invalidate entire TLB */
void vm_flush_tlb(void)
{
    __asm__ volatile("tlbi VMALLE1");
    dsb();
    isb();
}

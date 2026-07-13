/*
 * entry.S — TorOS ARM64 boot girişi
 *
 * QEMU -kernel ile iki modda da çalışır:
 *   1) Raw Image (Linux boot protokolü, "ARM\x64" header)
 *   2) ELF yükleme (entry = _start)
 *
 * Yapılanlar:
 *   - Image header (offset 0x38'de "ARM\x64" magic)
 *   - EL2 -> EL1 geçişi (gerekirse)
 *   - Secondary CPU park etme (SMP: sadece CPU0 init yapar)
 *   - BSS temizleme, stack kurulumu
 *   - kernel_main'e atlama
 */

.section .text.head, "ax"

.global _start
_start:
    /* ---------------- ARM64 Image header ---------------- */
    b       _entry                  /* code0: girişe dallan          */
    nop                             /* code1                          */
    .quad   0x80000                 /* text_offset (0x08)             */
    .quad   _kernel_size            /* image_size  (0x10)             */
    .quad   0                       /* flags       (0x18)             */
    .quad   0                       /* res2        (0x20)             */
    .quad   0                       /* res3        (0x28)             */
    .quad   0                       /* res4        (0x30)             */
    .ascii  "ARM\x64"               /* magic       (0x38)             */
    .long   0                       /* res5        (0x3C)             */

_entry:
    /* x0 = DTB pointer (şimdilik saklıyoruz) */
    mov     x20, x0

    /* ---------------- EL tespiti ---------------- */
    mrs     x9, CurrentEL
    lsr     x9, x9, #2
    cmp     x9, #2
    b.ne    .Lel1

    /* ---------------- EL2 -> EL1 ---------------- */
    /* HCR_EL2.RW = 1 (EL1 AArch64) */
    mov     x9, #(1 << 31)
    msr     hcr_el2, x9

    /* Timer sayaçlarına EL1 erişimi */
    mov     x9, #3                  /* EL1PCEN | EL1PCTEN */
    msr     cnthctl_el2, x9
    msr     cntvoff_el2, xzr

    /* FP/SIMD tuzaklarını kapat */
    msr     cptr_el2, xzr
    mov     x9, #0x33ff
    msr     hstr_el2, xzr

    /* GICv3 sistem register erişimi (EL2'den EL1'e) */
    mov     x9, #1
    msr     icc_sre_el2, x9

    /* SPSR_EL2 = EL1h, DAIF maskeli */
    mov     x9, #0x3c5
    msr     spsr_el2, x9
    adr     x9, .Lel1
    msr     elr_el2, x9
    eret

.Lel1:
    /* FP/SIMD'yi EL1'de serbest bırak (FPEN = 0b11) */
    mov     x9, #(3 << 20)
    msr     cpacr_el1, x9
    isb

    /* ---------------- SMP: secondary CPU park ---------------- */
    mrs     x9, mpidr_el1
    and     x9, x9, #0xff           /* Aff0 */
    cbz     x9, .Lprimary

.Lpark:
    wfe
    b       .Lpark

.Lprimary:
    /* ---------------- Vektör tabanı ---------------- */
    adr     x9, exception_vectors
    msr     vbar_el1, x9
    isb

    /* ---------------- BSS temizle ---------------- */
    ldr     x9, =__bss_start
    ldr     x10, =__bss_end
.Lbss_loop:
    cmp     x9, x10
    b.hs    .Lbss_done
    str     xzr, [x9], #8
    b       .Lbss_loop
.Lbss_done:

    /* ---------------- Boot stack ---------------- */
    ldr     x9, =__stack_top
    and     x9, x9, #~0xf
    mov     sp, x9

    /* DTB'yi korumaya devam (x20) */
    mov     x0, x20
    bl      kernel_main

.Lhang:
    wfe
    b       .Lhang

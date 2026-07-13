/*
 * vectors.S — EL1 istisna vektör tablosu
 *
 * 16 giriş (4 durum x 4 tip), her biri TAM 0x80 bayt (32 komut).
 * Her slot sadece işleyici gövdesine dallanır (gövdeler tablo sonunda).
 *
 * Trap frame: x0..x30 + ELR + SPSR + pad = 272 bayt (irq.h ile aynı).
 */

#define TF_SIZE 272

.macro SAVE_REGS
    sub     sp, sp, #TF_SIZE
    stp     x0,  x1,  [sp, #16*0]
    stp     x2,  x3,  [sp, #16*1]
    stp     x4,  x5,  [sp, #16*2]
    stp     x6,  x7,  [sp, #16*3]
    stp     x8,  x9,  [sp, #16*4]
    stp     x10, x11, [sp, #16*5]
    stp     x12, x13, [sp, #16*6]
    stp     x14, x15, [sp, #16*7]
    stp     x16, x17, [sp, #16*8]
    stp     x18, x19, [sp, #16*9]
    stp     x20, x21, [sp, #16*10]
    stp     x22, x23, [sp, #16*11]
    stp     x24, x25, [sp, #16*12]
    stp     x26, x27, [sp, #16*13]
    stp     x28, x29, [sp, #16*14]
    str     x30,      [sp, #240]
    mrs     x9, elr_el1
    mrs     x10, spsr_el1
    stp     x9, x10,  [sp, #248]
.endm

.macro RESTORE_REGS
    ldp     x9, x10,  [sp, #248]
    msr     elr_el1, x9
    msr     spsr_el1, x10
    ldr     x30,      [sp, #240]
    ldp     x0,  x1,  [sp, #16*0]
    ldp     x2,  x3,  [sp, #16*1]
    ldp     x4,  x5,  [sp, #16*2]
    ldp     x6,  x7,  [sp, #16*3]
    ldp     x8,  x9,  [sp, #16*4]
    ldp     x10, x11, [sp, #16*5]
    ldp     x12, x13, [sp, #16*6]
    ldp     x14, x15, [sp, #16*7]
    ldp     x16, x17, [sp, #16*8]
    ldp     x18, x19, [sp, #16*9]
    ldp     x20, x21, [sp, #16*10]
    ldp     x22, x23, [sp, #16*11]
    ldp     x24, x25, [sp, #16*12]
    ldp     x26, x27, [sp, #16*13]
    ldp     x28, x29, [sp, #16*14]
    add     sp, sp, #TF_SIZE
.endm

/* Her slot: tek dallanma komutu (0x80'den kesinlikle küçük) */
.macro VENTRY target
    .balign 0x80
    b       \target
.endm

/* ---------------------------------------------------------- */
.section .text.vectors, "ax"
.balign 2048
.global exception_vectors
exception_vectors:
    /* Current EL, SP0 */
    VENTRY sync_cur_sp0
    VENTRY irq_cur_sp0
    VENTRY fiq_cur_sp0
    VENTRY serror_cur_sp0
    /* Current EL, SPx */
    VENTRY sync_cur_spx
    VENTRY irq_cur_spx
    VENTRY fiq_cur_spx
    VENTRY serror_cur_spx
    /* Lower EL, AArch64 */
    VENTRY sync_low_a64
    VENTRY irq_low_a64
    VENTRY fiq_low_a64
    VENTRY serror_low_a64
    /* Lower EL, AArch32 */
    VENTRY sync_low_a32
    VENTRY irq_low_a32
    VENTRY fiq_low_a32
    VENTRY serror_low_a32

/* ---------------- İşleyici gövdeleri ---------------- */

.macro SYNC_BODY kind
    SAVE_REGS
    mov     x0, sp
    mov     x1, #\kind
    bl      sync_exception_handler
    b       .
.endm

.macro FIQ_BODY kind
    SAVE_REGS
    mov     x0, sp
    mov     x1, #\kind
    bl      fiq_exception_handler
    b       .
.endm

.macro SERROR_BODY kind
    SAVE_REGS
    mov     x0, sp
    mov     x1, #\kind
    bl      serror_exception_handler
    b       .
.endm

.macro IRQ_BODY
    SAVE_REGS
    mov     x0, sp
    bl      irq_exception_handler   /* dönüş: kullanılacak trap frame */
    mov     sp, x0
    RESTORE_REGS
    eret
.endm

sync_cur_sp0:    SYNC_BODY 0
irq_cur_sp0:     IRQ_BODY
fiq_cur_sp0:     FIQ_BODY 0
serror_cur_sp0:  SERROR_BODY 0

sync_cur_spx:    SYNC_BODY 1
irq_cur_spx:     IRQ_BODY
fiq_cur_spx:     FIQ_BODY 1
serror_cur_spx:  SERROR_BODY 1

sync_low_a64:    SYNC_BODY 2
irq_low_a64:     IRQ_BODY
fiq_low_a64:     FIQ_BODY 2
serror_low_a64:  SERROR_BODY 2

sync_low_a32:    SYNC_BODY 3
irq_low_a32:     IRQ_BODY
fiq_low_a32:     FIQ_BODY 3
serror_low_a32:  SERROR_BODY 3

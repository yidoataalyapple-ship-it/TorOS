/*
 * torOS Exception/Interrupt Vectors (AArch64)
 * VBAR_EL1 points here
 * 4 exception levels x 4 stack types = 16 entries, each 128 bytes
 */

.section .text

/* Save all registers to stack */
.macro SAVE_REGS
    sub sp, sp, #272
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs x0, ELR_EL1
    str x0, [sp, #248]
    mrs x0, SPSR_EL1
    str x0, [sp, #256]
.endm

/* Restore all registers */
.macro RESTORE_REGS
    ldr x0, [sp, #248]
    msr ELR_EL1, x0
    ldr x0, [sp, #256]
    msr SPSR_EL1, x0
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #272
.endm

/* Vector entry - align to 128 bytes */
.macro VENTRY name
    .align 7
    b \name
.endm

/* Global vectors */
.globl vector_table
.align 11
vector_table:
    /* Current EL with SP0 */
    VENTRY sync_invalid_el0
    VENTRY irq_invalid_el0
    VENTRY fiq_invalid_el0
    VENTRY error_invalid_el0

    /* Current EL with SPx */
    VENTRY sync_el1
    VENTRY irq_el1
    VENTRY fiq_el1
    VENTRY error_el1

    /* Lower EL using AArch64 */
    VENTRY sync_invalid_el0_64
    VENTRY irq_invalid_el0_64
    VENTRY fiq_invalid_el0_64
    VENTRY error_invalid_el0_64

    /* Lower EL using AArch32 */
    VENTRY sync_invalid_el0_32
    VENTRY irq_invalid_el0_32
    VENTRY fiq_invalid_el0_32
    VENTRY error_invalid_el0_32

/* Synchronous exception at EL1 */
.align 7
sync_el1:
    SAVE_REGS
    mov x0, sp
    bl trap_handle
    RESTORE_REGS
    eret

/* IRQ at EL1 */
.align 7
irq_el1:
    SAVE_REGS
    bl irq_handle
    RESTORE_REGS
    eret

/* FIQ at EL1 */
.align 7
fiq_el1:
    SAVE_REGS
    bl fiq_handle
    RESTORE_REGS
    eret

/* Error at EL1 */
.align 7
error_el1:
    SAVE_REGS
    bl error_handle
    RESTORE_REGS
    eret

/* Invalid handlers - should not happen */
.align 7
sync_invalid_el0:
irq_invalid_el0:
fiq_invalid_el0:
error_invalid_el0:
sync_invalid_el0_64:
irq_invalid_el0_64:
fiq_invalid_el0_64:
error_invalid_el0_64:
sync_invalid_el0_32:
irq_invalid_el0_32:
fiq_invalid_el0_32:
error_invalid_el0_32:
    b panic_vector

panic_vector:
    /* Infinite loop with wfi */
1:
    wfi
    b 1b

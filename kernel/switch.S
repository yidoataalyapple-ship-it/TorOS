/*
 * torOS Context Switch
 * ARM64 Assembly - saves/restores full register context
 * 
 * void context_switch(context_t *old, context_t *new)
 * Saves current context to 'old', restores from 'new'
 */

.section .text
.global context_switch
.global context_first_switch
.global kernel_to_user
.global user_to_kernel

/*
 * context_switch(context_t *old_ctx, context_t *new_ctx)
 * x0 = old context pointer
 * x1 = new context pointer
 */
context_switch:
    // Save callee-saved registers to old context
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]     // fp, lr
    mov x2, sp
    str x2,      [x0, #96]      // sp

    // Restore callee-saved registers from new context
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]     // fp, lr
    ldr x2,      [x1, #96]      // sp
    mov sp, x2

    ret

/*
 * context_first_switch(context_t *new_ctx)
 * Used when switching to a newly created process
 * x0 = new context pointer
 */
context_first_switch:
    // Restore callee-saved registers
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldp x29, x30, [x0, #80]
    ldr x2,      [x0, #96]
    mov sp, x2

    // Enable interrupts and return to user
    msr DAIFClr, #0xF
    ret

/*
 * kernel_to_user(uint64 user_stack, uint64 user_entry, uint64 user_arg)
 * Switch from kernel EL1 to user EL0
 * x0 = user stack pointer
 * x1 = user entry point
 * x2 = user argument
 */
kernel_to_user:
    // Save kernel SP
    msr SP_EL0, x0

    // Setup SPSR for EL0
    // M[3:0] = 0x0 = EL0t (EL0 with SP0)
    mov x3, #0x0
    msr SPSR_EL1, x3

    // Set ELR to user entry
    msr ELR_EL1, x1

    // User argument in x0
    mov x0, x2

    // Enable interrupts
    msr DAIFClr, #0xF

    // Eret will set PC=ELR_EL1, PSTATE=SPSR_EL1, SP=SP_EL0
    eret

/*
 * user_to_kernel()
 * Called when user code triggers exception (syscall, fault)
 * Returns via eret from trap handler
 */
user_to_kernel:
    // This is the landing point from user mode
    // The trap handler already saved all registers
    // Just return to the trap handler
    ret

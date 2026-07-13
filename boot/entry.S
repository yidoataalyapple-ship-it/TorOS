/*
 * torOS Bootloader Entry Point
 * ARM64 (AArch64) - QEMU virt machine
 * Bare-metal startup - loaded at 0x40000000
 */

.section .text.boot
.globl _start

_start:
    // --- All cores enter here ---
    // x0 = device tree blob pointer (from QEMU)
    
    // Disable interrupts
    msr DAIFSet, #0xF       // Disable all interrupts
    
    // Check CPU ID (MPIDR_EL1)
    mrs x1, MPIDR_EL1
    and x1, x1, #0xFF       // CPU ID in lower 8 bits
    cbnz x1, park           // Only CPU 0 continues, others park
    
    // === CPU 0 ONLY ===
    
    // Save device tree pointer
    adrp x2, _dtb_ptr
    add x2, x2, :lo12:_dtb_ptr
    str x0, [x2]
    
    // Setup stack (grows downward)
    ldr x1, =_stack_top
    mov sp, x1
    
    // Clear BSS
    ldr x1, =_bss_start
    ldr x2, =_bss_end
    mov x3, #0
1:
    cmp x1, x2
    b.ge 2f
    str x3, [x1], #8
    b 1b
2:
    // Jump to C kernel
    bl kernel_main
    
    // Should never return
    b park

// Park secondary cores
park:
    wfe                     // Wait For Event (low power)
    b park

// Device tree pointer
.section .data
.global _dtb_ptr
.align 3
_dtb_ptr:
    .quad 0

// Early stack (32KB)
.section .bss.stack
.align 12
_stack_bottom:
    .space 32768
_stack_top:

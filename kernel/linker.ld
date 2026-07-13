/*
 * torOS Kernel Linker Script - ARM64 (AArch64)
 * QEMU virt machine
 */

OUTPUT_ARCH(aarch64)
ENTRY(_start)

MEMORY
{
    /* QEMU virt: 2GB RAM at 0x40000000 */
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 2048M
}

SECTIONS
{
    . = 0x40000000;

    /* Boot code first */
    .text : {
        PROVIDE(_text_start = .);
        *(.text.boot)
        *(.text .text.*)
        . = ALIGN(4096);
        PROVIDE(_text_end = .);
    } > RAM

    /* Read-only data */
    .rodata : {
        PROVIDE(_rodata_start = .);
        *(.rodata .rodata.*)
        . = ALIGN(4096);
        PROVIDE(_rodata_end = .);
    } > RAM

    /* Initialized data */
    .data : {
        PROVIDE(_data_start = .);
        *(.data .data.*)
        . = ALIGN(4096);
        PROVIDE(_data_end = .);
    } > RAM

    /* BSS */
    .bss : {
        PROVIDE(_bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4096);
        PROVIDE(_bss_end = .);
    } > RAM

    PROVIDE(_kernel_end = .);
    PROVIDE(_stack_top = 0x40000000 + 2048M);
}

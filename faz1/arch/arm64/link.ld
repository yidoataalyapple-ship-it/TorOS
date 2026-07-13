/*
 * link.ld — TorOS kernel linker script
 * QEMU virt: RAM tabanı 0x40000000, Image text_offset 0x80000
 */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

KERNEL_BASE = 0x40080000;

SECTIONS
{
    . = KERNEL_BASE;
    __kernel_start = .;

    .text : {
        *(.text.head)
        *(.text.vectors)
        *(.text*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(4096);
    .data : {
        *(.data*)
    }

    . = ALIGN(4096);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(4096);
    __bss_end = .;

    /* Boot stack (64KB) */
    . = ALIGN(16);
    __stack_bottom = .;
    . += 0x10000;
    __stack_top = .;

    /* Page table alanı (mmu.c statik de kullanabilir, burada rezerve) */
    . = ALIGN(4096);
    __kernel_end = .;
    _kernel_size = __kernel_end - __kernel_start;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.debug*)
    }
}

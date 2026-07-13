# torOS

<p align="center">
  <img src="https://img.shields.io/badge/Arch-AArch64%20(ARM64)-blue?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/Type-Bare--Metal-red?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/Version-0.2.0-orange?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge"/>
</p>

**torOS** is a modern, from-scratch operating system kernel for ARM64 (AArch64) architecture. Built entirely in Assembly and C, torOS runs bare-metal on QEMU with a real MMU, GICv3 interrupt controller, preemptive scheduler, virtual memory, framebuffer graphics, and an interactive shell.

---

## What's New in v0.2.0

- **Virtual Memory** - Full 4-level page table MMU with 48-bit addressing
- **GICv3** - Generic Interrupt Controller v3 with distributor, redistributor, and CPU interface
- **Framebuffer** - 1024x768 graphics with 8x8 bitmap font rendering
- **Context Switch** - Assembly context switch for true preemptive multitasking
- **User Programs** - hello, counter, primes programs with `run` and `userlist` commands
- **Shell v0.2** - New commands: mmu, gfx, userlist, run

## Architecture

```
+------------------------------+
|     User Programs (EL0)      |
|  hello, counter, primes      |
+------------------------------+
|   System Call Interface      |
+------------------------------+
|   Process Scheduler (RR)     |
+------------------------------+
|  GICv3 + Trap/IRQ Handler    |
|  VBAR_EL1 + Generic Timer    |
+------------------------------+
|   Virtual Memory (MMU)       |
|   4-Level Page Tables        |
+------------------------------+
|   Memory Manager             |
|   Page Alloc + kmalloc       |
+------------------------------+
|   Framebuffer 1024x768       |
|   UART PL011 Console         |
+------------------------------+
|   ARM64 Bootloader           |
|   Multi-core entry.S         |
+------------------------------+
|   QEMU virt machine          |
+------------------------------+
```

## Features

| Component | Status | Description |
|-----------|--------|-------------|
| **Bootloader** | Ready | AArch64 assembly, multi-core bring-up |
| **UART** | Ready | PL011 serial driver @ 115200 baud |
| **printk** | Ready | Formatted output with ANSI colors |
| **Memory Manager** | Ready | Page allocator, bitmap, 4MB kmalloc heap |
| **MMU / VM** | **NEW** | 4-level page tables, identity mapping, caches |
| **GICv3** | **NEW** | Full interrupt controller support |
| **Timer** | Ready | Generic Timer @ 100Hz |
| **Scheduler** | Ready | Round-robin preemptive with sleep/yield |
| **Context Switch** | **NEW** | Full register save/restore in assembly |
| **Syscalls** | Ready | write, read, exit, fork, sleep, getpid, exec |
| **Shell** | Ready | 15+ commands with user program support |
| **Framebuffer** | **NEW** | 1024x768 @ 32bpp, lines, rects, text |
| **User Programs** | **NEW** | hello, counter, primes |

## Project Structure

```
torOS/
├── boot/
│   └── entry.S              # Bootloader & CPU bring-up
├── kernel/
│   ├── linker.ld            # Memory layout
│   ├── kernel_main.c        # Kernel entry
│   ├── uart.c               # PL011 UART driver
│   ├── printk.c             # Formatted output + logo
│   ├── mm.c                 # Physical memory manager
│   ├── vm.c                 # Virtual memory / MMU
│   ├── gic.c                # GICv3 interrupt controller
│   ├── trap.S               # Exception vectors (ASM)
│   ├── trap.c               # Trap/IRQ/timer handlers
│   ├── switch.S             # Context switch (ASM)
│   ├── sched.c              # Process scheduler
│   ├── syscall.c            # System call handler
│   ├── fb.c                 # Framebuffer driver
│   ├── user.c               # User mode programs
│   └── shell.c              # Interactive shell
├── libc/
│   └── string.c             # String utilities
├── include/
│   └── toros.h              # Kernel header
├── Makefile
└── README.md
```

## Building

### Prerequisites
```bash
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
                 qemu-system-arm make
```

### Compile & Run
```bash
make clean
make
make run
```

### Debug with GDB
```bash
make debug        # Terminal 1
aarch64-linux-gnu-gdb -ex "target remote localhost:1234" \
  -ex "symbol-file build/toros.elf" -ex "continue"   # Terminal 2
```

### Exit QEMU
Press `Ctrl+A` then `X`

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `clear` | Clear screen |
| `uname` | System information |
| `free` | Memory usage |
| `ps` | Process list |
| `echo` | Print text |
| `uptime` | System uptime |
| `colors` | Color test |
| `kmap` | Kernel memory map |
| `mmu` | **MMU status** |
| `userlist` | **List user programs** |
| `run <prog>` | **Run user program** |
| `gfx` | **Graphics test** |
| `reboot` | Reboot |
| `halt` | Shutdown |

## Technical Specs

| Spec | Value |
|------|-------|
| Architecture | ARM64 (AArch64) |
| CPU | Cortex-A72 |
| Cores | 4 |
| RAM | 2GB |
| Page Size | 4KB |
| VA Space | 48-bit |
| Timer | Generic @ 100Hz |
| Console | PL011 UART |
| Graphics | 1024x768x32bpp |
| GIC | GICv3 |

## Roadmap

- [x] Bootloader & multi-core
- [x] UART console
- [x] printk with formatting
- [x] Page allocator
- [x] **MMU / Virtual Memory**
- [x] **GICv3 Interrupt Controller**
- [x] Timer IRQ
- [x] Scheduler
- [x] **Context Switch**
- [x] Syscalls
- [x] **Framebuffer Graphics**
- [x] **User Programs**
- [x] Shell
- [ ] File System
- [ ] Keyboard driver
- [ ] SMP scheduling
- [ ] Networking

## License

MIT License

---

<p align="center"><b>Built from scratch with passion</b></p>

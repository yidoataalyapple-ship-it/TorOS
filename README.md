# torOS

<p align="center">
  <img src="https://img.shields.io/badge/Arch-AArch64%20(ARM64)-blue?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/Type-Bare--Metal-red?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/Status-Alpha-orange?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge"/>
</p>

**torOS** is a modern, from-scratch operating system kernel for ARM64 (AArch64) architecture. Built entirely in Assembly and C, torOS runs bare-metal on QEMU and features a real preemptive scheduler, interrupt handling, memory management, system calls, and an interactive shell.

---

## Features

| Component | Status | Description |
|-----------|--------|-------------|
| **Bootloader** | Ready | Custom AArch64 assembly entry point, multi-core bring-up |
| **UART Driver** | Ready | PL011 serial driver for console I/O |
| **printk** | Ready | Full-featured formatted output with ANSI colors |
| **Memory Manager** | Ready | Page allocator (4KB), bitmap-based, kmalloc heap |
| **Interrupts** | Ready | VBAR_EL1 vector table, timer IRQ, exception handling |
| **Scheduler** | Ready | Round-robin preemptive multitasking, sleep/yield |
| **Process Manager** | Ready | Process table, process creation, state tracking |
| **System Calls** | Ready | Syscall interface (write, read, exit, fork, sleep, getpid, exec) |
| **Shell** | Ready | Interactive command-line with 10+ built-in commands |
| **Frame Buffer** | Planned | Graphics output |
| **File System** | Planned | Simple filesystem |
| **Keyboard** | Planned | PS/2 or USB keyboard driver |
| **Networking** | Planned | Basic network stack |

## Architecture

```
+------------------------------+
|       User Applications      |
|  (Shell + future user progs) |
+------------------------------+
|      System Call Interface   |
+------------------------------+
|      Process Scheduler       |
|   (Round-robin preemptive)   |
+------------------------------+
|    Trap / Interrupt Handler  |
|    (VBAR_EL1 vector table)   |
+------------------------------+
|      Memory Manager          |
|   (Page allocator + kmalloc) |
+------------------------------+
|      UART Console (PL011)    |
+------------------------------+
|      ARM64 Bootloader        |
|   (entry.S - multi-core)     |
+------------------------------+
|      QEMU virt machine       |
+------------------------------+
```

## Project Structure

```
torOS/
├── boot/
│   └── entry.S              # Bootloader & CPU bring-up
├── kernel/
│   ├── linker.ld            # Memory layout
│   ├── kernel_main.c        # Kernel entry point
│   ├── uart.c               # PL011 UART driver
│   ├── printk.c             # Formatted output
│   ├── mm.c                 # Memory manager
│   ├── trap.S               # Exception vectors (ASM)
│   ├── trap.c               # Trap/IRQ handlers
│   ├── sched.c              # Process scheduler
│   ├── syscall.c            # System call handler
│   └── shell.c              # Interactive shell
├── libc/
│   └── string.c             # String & memory utils
├── include/
│   └── toros.h              # Main kernel header
├── build/                   # Build artifacts
├── Makefile
└── README.md
```

## Building

### Prerequisites

- `aarch64-linux-gnu-gcc` (cross-compiler)
- `aarch64-linux-gnu-ld` (linker)
- `qemu-system-aarch64` (emulator)
- `make`

### Install cross-compiler (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-arm make
```

### Compile

```bash
make clean
make
```

Output: `build/toros.elf` (ELF) and `build/toros.bin` (raw binary)

## Running

### QEMU (recommended)

```bash
make run
```

Or manually:

```bash
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a72 \
  -m 2048 \
  -smp 4 \
  -nographic \
  -kernel build/toros.elf
```

### Exit QEMU

Press `Ctrl+A` then `X`

### Debug with GDB

Terminal 1:
```bash
make debug
```

Terminal 2:
```bash
aarch64-linux-gnu-gdb -ex "target remote localhost:1234" \
  -ex "symbol-file build/toros.elf" -ex "break kernel_main" \
  -ex "continue"
```

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show all available commands |
| `clear` | Clear the screen |
| `uname` | Show system information |
| `free` | Display memory usage |
| `ps` | List all processes |
| `echo <text>` | Print text to console |
| `uptime` | Show system uptime |
| `colors` | Display ANSI color test |
| `kmap` | Show kernel memory map |
| `reboot` | Reboot the system |
| `halt` | Halt the system |

## Memory Layout

```
0x4000_0000 +------------------+
            | .text.boot       | Boot code
            | .text            | Kernel code
            | .rodata          | Read-only data
            | .data            | Initialized data
            | .bss             | Uninitialized data
            +------------------+
            | Bitmap           | Page frame bitmap
            | Heap             | kmalloc heap (4MB)
            |                  |
            ~ Free Pages       ~ Available memory
            |                  |
0xBFFF_FFFF +------------------+ RAM End (2GB)
```

## Technical Details

| Spec | Value |
|------|-------|
| Architecture | ARM64 (AArch64) |
| CPU | Cortex-A72 (QEMU virt) |
| Cores | 4 (SMP ready) |
| RAM | 2GB |
| Page Size | 4KB |
| Timer | Generic Timer @ 100Hz |
| Console | PL011 UART @ 0x09000000 |
| Exception Level | EL1 |

## Development Roadmap

- [x] Bootloader & multi-core bring-up
- [x] UART serial console
- [x] printk with formatting
- [x] Page allocator
- [x] Interrupt/exception handling
- [x] Timer IRQ
- [x] Round-robin scheduler
- [x] Process management
- [x] System calls
- [x] Interactive shell
- [ ] Frame buffer / display
- [ ] Physical memory management (buddy allocator)
- [ ] Virtual memory (MMU, page tables)
- [ ] File system (VFS + ext2-like)
- [ ] User mode programs
- [ ] Keyboard driver
- [ ] Real-time clock
- [ ] SMP scheduling (multi-core)
- [ ] Networking stack
- [ ] POSIX compatibility layer

## Contributing

This is an educational and experimental project. Contributions are welcome!

## License

MIT License - See LICENSE file for details.

---

<p align="center">
  <b>Built from scratch with passion</b>
</p>

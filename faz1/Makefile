# TorOS Makefile
# Kullanım:
#   make                      - kernel.elf + kernel.img üret
#   make CROSS_COMPILE=aarch64-linux-gnu-
#   make run                  - QEMU'da çalıştır (klavye+fare aygıtlı)
#   make test                 - otomatik smoke test
#   make clean

CROSS_COMPILE ?= aarch64-none-elf-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

ARCH_FLAGS = -mcpu=cortex-a72 -mgeneral-regs-only -mstrict-align
CFLAGS  = $(ARCH_FLAGS) -ffreestanding -fno-builtin -nostdlib \
          -fno-stack-protector -fno-omit-frame-pointer \
          -O2 -Wall -Wextra -Wno-unused-parameter -Iinclude -g
ASFLAGS = $(ARCH_FLAGS) -Iinclude
LDFLAGS = -nostdlib -T arch/arm64/link.ld

# Kaynaklar
C_SRCS = $(wildcard kernel/*.c drivers/*.c input/*.c fs/*.c)
A_SRCS = arch/arm64/entry.S arch/arm64/vectors.S

OBJS = $(C_SRCS:.c=.o) $(A_SRCS:.S=.o)

all: kernel.elf kernel.img

kernel.elf: $(OBJS) arch/arm64/link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "==> kernel.elf hazır"
	@$(CROSS_COMPILE)size $@ 2>/dev/null || true

kernel.img: kernel.elf
	$(OBJCOPY) -O binary $< $@
	@echo "==> kernel.img hazır"

HEADERS = $(wildcard include/toros/*.h)
$(OBJS): $(HEADERS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

# ---------------- QEMU ----------------
QEMU ?= qemu-system-aarch64
QEMU_FLAGS = -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -m 2048 \
             -device virtio-keyboard-pci -device virtio-mouse-pci

run: kernel.elf
	$(QEMU) $(QEMU_FLAGS) -nographic -kernel kernel.elf

run-monitor: kernel.elf
	$(QEMU) $(QEMU_FLAGS) -display none -serial stdio \
	    -monitor unix:/tmp/toros-monitor.sock,server,nowait \
	    -kernel kernel.elf

test: kernel.elf
	python3 scripts/smoke_test.py --qemu $(QEMU) --kernel kernel.elf

disasm: kernel.elf
	$(OBJDUMP) -d kernel.elf | less

clean:
	rm -f $(OBJS) kernel.elf kernel.img

.PHONY: all run run-monitor test clean disasm

#
# torOS Build System
# ARM64 (AArch64) Bare-Metal OS
#

# Cross-compiler prefix
CROSS   = aarch64-linux-gnu

# Tools
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump
GDB     = $(CROSS)-gdb

# Directories
BOOT    = boot
KERNEL  = kernel
LIBC    = libc
INC     = include
BUILD   = build
ISO     = iso

# Source files
ASM_SRCS = $(BOOT)/entry.S \
           $(KERNEL)/trap.S

C_SRCS   = $(KERNEL)/kernel_main.c \
           $(KERNEL)/uart.c \
           $(KERNEL)/printk.c \
           $(KERNEL)/mm.c \
           $(KERNEL)/trap.c \
           $(KERNEL)/sched.c \
           $(KERNEL)/syscall.c \
           $(KERNEL)/shell.c \
           $(LIBC)/string.c

OBJS     = $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SRCS))) \
           $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SRCS)))

# Compiler flags
CFLAGS  = -Wall -Wextra -O2 -g
CFLAGS += -ffreestanding          # No standard library
CFLAGS += -nostdinc               # No standard includes
CFLAGS += -nostdlib               # No standard library linking
CFLAGS += -mgeneral-regs-only     # No floating point in kernel
CFLAGS += -mcmodel=large          # Large memory model
CFLAGS += -I$(INC)                # Include directory
CFLAGS += -DTOROS_VER=\"0.1.0\"
CFLAGS += -DTOROS_BUILD=\"$(shell date +%Y%m%d)\" 

# Assembler flags
ASFLAGS = -I$(INC) -g

# Linker flags
LDFLAGS = -T $(KERNEL)/linker.ld
LDFLAGS += -nostdlib

# Target
TARGET  = $(BUILD)/toros.elf
BIN     = $(BUILD)/toros.bin
ISO_IMG = $(BUILD)/toros.iso

# QEMU settings
QEMU    = qemu-system-aarch64
QFLAGS  = -machine virt          \
          -cpu cortex-a72        \
          -m 2048                \
          -smp 4                 \
          -nographic             \
          -kernel $(TARGET)      \
          -serial stdio

# GDB settings
GDBFLAGS = -ex "target remote localhost:1234" \
           -ex "symbol-file $(TARGET)"         \
           -ex "break kernel_main"             \
           -ex "continue"

# ---- Rules ----

.PHONY: all clean run debug iso push dump

all: $(BUILD) $(TARGET) $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

# Assembly
$(BUILD)/entry.o: $(BOOT)/entry.S
	@echo "  AS      $<"
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/trap.o: $(KERNEL)/trap.S
	@echo "  AS      $<"
	$(CC) $(ASFLAGS) -c $< -o $@

# C files - boot
$(BUILD)/kernel_main.o: $(KERNEL)/kernel_main.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: $(KERNEL)/uart.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/printk.o: $(KERNEL)/printk.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mm.o: $(KERNEL)/mm.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/trap_c.o: $(KERNEL)/trap.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sched.o: $(KERNEL)/sched.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: $(KERNEL)/syscall.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: $(KERNEL)/shell.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/string.o: $(LIBC)/string.c
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ELF
$(TARGET): $(OBJS)
	@echo "  LD      $@"
	$(LD) $(LDFLAGS) -o $@ $^

# Binary
$(BIN): $(TARGET)
	@echo "  BIN     $@"
	$(OBJCOPY) -O binary $< $@

# Disassembly
dump: $(TARGET)
	$(OBJDUMP) -d $(TARGET) > $(BUILD)/toros.asm
	@echo "Disassembly: $(BUILD)/toros.asm"

# ---- QEMU ----

run: $(TARGET)
	$(QEMU) $(QFLAGS)

debug: $(TARGET)
	$(QEMU) $(QFLAGS) -S -gdb tcp::1234

# ---- ISO ----

iso: $(BIN)
	mkdir -p $(ISO)/boot/grub
	cp $(BIN) $(ISO)/boot/
	echo 'set timeout=0' > $(ISO)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO)/boot/grub/grub.cfg
	echo 'menuentry "torOS" {' >> $(ISO)/boot/grub/grub.cfg
	echo '  multiboot /boot/toros.bin' >> $(ISO)/boot/grub/grub.cfg
	echo '  boot' >> $(ISO)/boot/grub/grub.cfg
	echo '}' >> $(ISO)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMG) $(ISO)/ 2>/dev/null || echo "ISO creation requires grub-mkrescue"

# ---- Clean ----

clean:
	rm -rf $(BUILD) $(ISO)

# ---- GitHub Push ----

push:
	@echo "Pushing to GitHub..."
	git add -A
	@read -p "Commit message: " msg; \
	git commit -m "$$msg" || true
	git push origin main

#
# TorOS Build System v0.4.0
# ARM64 Bare-Metal Operating System
# Full Desktop OS with GUI, Networking, Audio, Security
#

CROSS   = aarch64-linux
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump

BOOT    = boot
KERNEL  = kernel
LIBC    = libc
INC     = include
BUILD   = build

# Assembly sources
ASM_SRCS = $(BOOT)/entry.S \
           $(KERNEL)/trap.S \
           $(KERNEL)/switch.S

# C kernel sources - Core
C_SRCS  = $(KERNEL)/kernel_main.c \
          $(KERNEL)/uart.c \
          $(KERNEL)/printk.c \
          $(KERNEL)/mm.c \
          $(KERNEL)/vm.c \
          $(KERNEL)/gic.c \
          $(KERNEL)/trap.c \
          $(KERNEL)/sched.c \
          $(KERNEL)/syscall.c \
          $(KERNEL)/shell.c \
          $(KERNEL)/shell_gui.c \
          $(KERNEL)/fb.c \
          $(KERNEL)/rtc.c \
          $(KERNEL)/user.c \
          $(KERNEL)/spinlock.c \
          $(KERNEL)/smp.c

# C kernel sources - Input
C_SRCS += $(KERNEL)/input_event.c \
          $(KERNEL)/virtio_input.c \
          $(KERNEL)/usb_xhci.c

# C kernel sources - File System
C_SRCS += $(KERNEL)/fs.c \
          $(KERNEL)/fs_advanced.c

# C kernel sources - Graphics
C_SRCS += $(KERNEL)/virtio_gpu.c \
          $(KERNEL)/gpu_buffer.c \
          $(KERNEL)/dmabuf.c \
          $(KERNEL)/clip.c \
          $(KERNEL)/compositor.c \
          $(KERNEL)/window.c \
          $(KERNEL)/desktop.c \
          $(KERNEL)/virtual_desktop.c

# C kernel sources - UI Toolkit
C_SRCS += $(KERNEL)/widget.c \
          $(KERNEL)/widget_views.c \
          $(KERNEL)/widget_tree.c \
          $(KERNEL)/font.c \
          $(KERNEL)/image.c

# C kernel sources - Audio
C_SRCS += $(KERNEL)/audio.c

# C kernel sources - Network
C_SRCS += $(KERNEL)/network.c \
          $(KERNEL)/network_sock.c

# C kernel sources - Security
C_SRCS += $(KERNEL)/security.c

# C kernel sources - Init & Debug
C_SRCS += $(KERNEL)/init.c \
          $(KERNEL)/debug.c \
          $(KERNEL)/debug_log.c

# C kernel sources - Applications
C_SRCS += $(KERNEL)/app.c \
          $(KERNEL)/app_extra.c

# C library sources
C_SRCS += $(LIBC)/string.c

# trap.c must become trap_c.o (trap.S already claims trap.o)
C_SRCS_NO_TRAP = $(filter-out $(KERNEL)/trap.c,$(C_SRCS))
OBJS = $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SRCS))) \
       $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SRCS_NO_TRAP))) \
       $(BUILD)/trap_c.o

CFLAGS  = -Wall -Wextra -O2 -g -ffreestanding -nostdinc -nostdlib -fno-stack-protector \
          -mcmodel=large -fno-pic -fno-pie -I$(INC) -DTOROS_VER=\"0.4.0\"

ASFLAGS = -g
LDFLAGS = -T $(KERNEL)/linker.ld -nostdlib

TARGET  = $(BUILD)/toros.elf
BIN     = $(BUILD)/toros.bin

QEMU = qemu-system-aarch64
QFLAGS = -machine virt,gic-version=3 -cpu cortex-a72 -m 2048 -smp 4 \
         -nographic -kernel $(TARGET) -serial stdio \
         -device virtio-keyboard-pci \
         -device virtio-mouse-pci \
         -device virtio-gpu-pci \
         -device virtio-net-pci,netdev=net0 \
         -netdev user,id=net0,hostfwd=tcp::8080-:80 \
         -device usb-ehci,id=ehci \
         -device usb-kbd,bus=ehci.0 \
         -device usb-mouse,bus=ehci.0

.PHONY: all clean run debug dump count

all: $(BUILD) $(TARGET) $(BIN)

$(BUILD):
	@mkdir -p $(BUILD)

# Assembly compilation
$(BUILD)/%.o: $(BOOT)/%.S | $(BUILD)
	@echo "  AS      $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# Kernel assembly
$(BUILD)/%.o: $(KERNEL)/%.S | $(BUILD)
	@echo "  AS      $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

# Kernel C files
$(BUILD)/%.o: $(KERNEL)/%.c | $(BUILD)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Library C files
$(BUILD)/%.o: $(LIBC)/%.c | $(BUILD)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Special rule for trap.c
$(BUILD)/trap_c.o: $(KERNEL)/trap.c | $(BUILD)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(TARGET)
	@echo "  BIN     $@"
	@$(OBJCOPY) -O binary $< $@

dump: $(TARGET)
	@$(OBJDUMP) -d $(TARGET) > $(BUILD)/toros.asm

run: $(TARGET)
	@echo "  QEMU    TorOS v0.4.0"
	@echo "  Devices: virtio-keyboard, virtio-mouse, virtio-gpu, virtio-net"
	@echo "  Audio:   AC'97 / HDA / VirtIO-Sound"
	@echo "  Network: User mode (10.0.2.15/24)"
	@$(QEMU) $(QFLAGS)

debug: $(TARGET)
	@$(QEMU) $(QFLAGS) -S -gdb tcp::1234

clean:
	@rm -rf $(BUILD)

count:
	@echo "Source files: $$(echo $(C_SRCS) $(ASM_SRCS) | wc -w)"
	@echo "Headers:      $$(ls $(INC)/*.h 2>/dev/null | wc -l)"
	@echo "Total LOC:    $$(cat $(C_SRCS) $(ASM_SRCS) $(INC)/*.h 2>/dev/null | wc -l)"

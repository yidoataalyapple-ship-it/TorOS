#
# torOS Build System v0.3
# ARM64 Bare-Metal OS
#

CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy
OBJDUMP = $(CROSS)-objdump

BOOT    = boot
KERNEL  = kernel
LIBC    = libc
INC     = include
BUILD   = build

ASM_SRCS = $(BOOT)/entry.S $(KERNEL)/trap.S $(KERNEL)/switch.S
C_SRCS   = $(KERNEL)/kernel_main.c $(KERNEL)/uart.c $(KERNEL)/printk.c \
           $(KERNEL)/mm.c $(KERNEL)/trap.c $(KERNEL)/sched.c \
           $(KERNEL)/syscall.c $(KERNEL)/shell.c $(KERNEL)/vm.c \
           $(KERNEL)/gic.c $(KERNEL)/fb.c $(KERNEL)/user.c \
           $(KERNEL)/spinlock.c $(KERNEL)/smp.c $(KERNEL)/fs.c \
           $(KERNEL)/rtc.c $(LIBC)/string.c

OBJS = $(patsubst %.S,$(BUILD)/%.o,$(notdir $(ASM_SRCS))) \
       $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SRCS)))

CFLAGS  = -Wall -Wextra -O2 -g -ffreestanding -nostdinc -nostdlib \
          -mgeneral-regs-only -mcmodel=large -I$(INC) -DTOROS_VER=\"0.3.0\"
ASFLAGS = -g
LDFLAGS = -T $(KERNEL)/linker.ld -nostdlib

TARGET  = $(BUILD)/toros.elf
BIN     = $(BUILD)/toros.bin

QEMU = qemu-system-aarch64
QFLAGS = -machine virt,gic-version=3 -cpu cortex-a72 -m 2048 -smp 4 \
         -nographic -kernel $(TARGET) -serial stdio

.PHONY: all clean run debug dump count

all: $(BUILD) $(TARGET) $(BIN)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: $(BOOT)/%.S | $(BUILD)
	@echo "  AS      $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: $(KERNEL)/%.S | $(BUILD)
	@echo "  AS      $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: $(KERNEL)/%.c | $(BUILD)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(LIBC)/%.c | $(BUILD)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

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
	@echo "  QEMU    torOS v0.3"
	@$(QEMU) $(QFLAGS)

debug: $(TARGET)
	@$(QEMU) $(QFLAGS) -S -gdb tcp::1234

clean:
	@rm -rf $(BUILD)

count:
	@echo "Files: $$(echo $(C_SRCS) $(ASM_SRCS) | wc -w)"
	@echo "LOC:   $$(cat $(C_SRCS) $(ASM_SRCS) $(INC)/toros.h 2>/dev/null | wc -l)"

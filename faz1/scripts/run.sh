#!/bin/sh
# TorOS'u QEMU'da çalıştır (serial + monitor soketli)
# Klavye/fare girişi için QEMU monitor'ünü kullanın:
#   socat - UNIX-CONNECT:/tmp/toros-monitor.sock
#   (qemu) sendkey a
#   (qemu) mouse_move 10 10
set -e
cd "$(dirname "$0")/.."

QEMU=${QEMU:-qemu-system-aarch64}

exec "$QEMU" \
    -machine virt,gic-version=3 \
    -cpu cortex-a72 \
    -smp 4 \
    -m 2048 \
    -device virtio-keyboard-pci \
    -device virtio-mouse-pci \
    -display none \
    -serial stdio \
    -monitor unix:/tmp/toros-monitor.sock,server,nowait \
    -kernel kernel.elf

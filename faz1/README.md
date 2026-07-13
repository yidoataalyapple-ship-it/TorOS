# TorOS — Faz 1 (Input Devices)

**ARM64 deneysel işletim sistemi** — QEMU `virt` makinesi üzerinde çalışan,
sıfırdan yazılmış bare-metal kernel. Uzun vadeli hedef: grafik arayüzlü,
pencere yönetimli, ağ destekli tam bir masaüstü OS.

> Bu dizin (`faz1/`), `faz1-input` dalında **QEMU'da boot'u ve 11/11 otomatik
> smoke testi doğrulanmış** bağımsız bir Faz 1 implementasyonudur.

```
  ████████╗ ██████╗ ██████╗  ██████╗ ███████╗
  ╚══██╔══╝██╔═══██╗██╔══██╗██╔═══██╗██╔════╝
     ██║   ██║   ██║██████╔╝██║   ██║███████╗
     ██║   ██║   ██║██╔══██╗██║   ██║╚════██║
     ██║   ╚██████╔╝██║  ██║╚██████╔╝███████║
     ╚═╝    ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
```

## Mevcut Durum: Faz 1 (Input Devices) ✅

Bu sürüm (`0.1.0-faz1`) planın **Faz 1**'ini uçtan uca, test edilmiş şekilde
içerir — ayrıca planda "mevcut" sayılan tüm temel kernel bileşenleri de
sıfırdan yazılmıştır:

| Bileşen | Açıklama |
|---|---|
| **Boot** | ARM64 Image header, EL2→EL1 geçişi, SMP park (4 çekirdek) |
| **MMU** | 4 seviyeli sayfa tablosu, 48-bit VA, identity map (RAM normal / MMIO device) |
| **GICv3** | Distributor + Redistributor + ICC sistem register arayüzü |
| **Timer** | ARM Generic Timer (CNTP, PPI 30), 100 Hz tick |
| **Scheduler** | Round-robin önleyici (preemptive), trap-frame context switch |
| **PCI** | ECAM enumerasyonu, **BAR adres ataması** (firmware'siz direct boot), INTx swizzle |
| **virtio** | **Modern (1.0 capability) + legacy** çift transport, vring yönetimi |
| **Faz 1.1** | virtio-input klavye: Linux keycode→ASCII, Shift/Ctrl/Alt/Caps takibi, blocking/non-blocking API |
| **Faz 1.2** | virtio-input fare: EV_REL delta→mutlak konum (1024x768 clamp), buton + wheel |
| **Faz 1.4** | Birleşik input event katmanı: 64-slot ring buffer, `/dev/input/eventN` sanal dosyaları |
| **torFS** | 64 dosya × 4KB in-memory dosya sistemi + aygıt dosyaları |
| **Shell** | UART + klavye girdili demo shell (`help` ile başlayın) |

## Derleme

Gereksinimler: `aarch64-linux-gnu-gcc` (veya `aarch64-none-elf-gcc`),
`qemu-system-aarch64`, `python3` (test için).

```bash
# Debian/Ubuntu:
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm

cd faz1
make CROSS_COMPILE=aarch64-linux-gnu-
# Çıktı: kernel.elf (ELF) + kernel.img (raw Image)
```

## Çalıştırma

```bash
make run        # -nographic, serial stdio
# veya monitor soketli:
make run-monitor
```

QEMU komut satırı:

```bash
qemu-system-aarch64 \
    -machine virt,gic-version=3 -cpu cortex-a72 -smp 4 -m 2048 \
    -device virtio-keyboard-pci -device virtio-mouse-pci \
    -nographic -kernel kernel.elf
```

### Klavye & fare girişi

- **Serial (UART):** doğrudan terminale yazın — shell UART'ı da dinler.
- **virtio klavye/fare:** QEMU monitor üzerinden olay enjekte edin:

```bash
socat - UNIX-CONNECT:/tmp/toros-monitor.sock
(qemu) sendkey h
(qemu) sendkey e
(qemu) sendkey l
(qemu) sendkey p
(qemu) sendkey ret
(qemu) mouse_move 100 50
(qemu) mouse_button 1
```

### Shell komutları

`help`, `ls`, `cat`, `write`, `rm`, `events [n]`, `mouse`, `kbdstat`,
`mem`, `ps`, `pci`, `uptime`, `uname`, `clear`

Örnek oturum:

```
toros> events 3
  [event0]   12.345000  KEY code= 30 value=1      <- 'a' bastınız
  [event0]   12.401000  SYN code=  0 value=0
  [event0]   12.510000  KEY code= 30 value=0
toros> mouse
Fare: x=612 y=434 butonlar=0x1 (L=1 R=0 M=0) wheel=0 olay=42
```

## Test

Otomatik smoke testi (boot + MMU/GIC/PCI log doğrulama + klavye `sendkey` +
fare `mouse_move` + torFS + canlı event okuma):

```bash
make test
```

CI şablonu: `docs/ci.yml` — GitHub Actions'a eklemek için repo kökünde
`.github/workflows/` altına kopyalayın.

## Mimari

```
┌──────────────────────────────────────────────┐
│  Shell task  │  Heartbeat task  │  idle       │   Scheduler (100Hz preemptive)
├──────────────────────────────────────────────┤
│  keyboard.c  │  mouse.c         │  event.c    │   Faz 1: input katmanı
│              virtio_input.c                   │   virtio-input sürücüsü
├──────────────────────────────────────────────┤
│  virtio.c (modern+legacy vring)  │  pci.c     │   ECAM + BAR alloc
├──────────────────────────────────────────────┤
│  gic.c │ timer.c │ irq.c │ vectors.S          │   Kesme altyapısı
├──────────────────────────────────────────────┤
│  mmu.c │ mm.c │ uart.c │ entry.S              │   Temel kernel
└──────────────────────────────────────────────┘
```

### Dizin yapısı

```
arch/arm64/     entry.S, vectors.S, link.ld
kernel/         main, uart, printf, mmu, gic, irq, timer, mm, sched, shell, string, panic
drivers/        pci.c, virtio.c, virtio_input.c
input/          event.c, keyboard.c, mouse.c, keymap.c
fs/             torfs.c
include/toros/  tüm başlıklar
scripts/        run.sh, smoke_test.py
docs/           FAZ-1-DURUM.md, ci.yml (GitHub Actions şablonu)
```

## Yol Haritası

- [x] **Faz 1** — Input devices (klavye/fare/touchpad altyapısı) ✅
- [ ] **Faz 2** — Grafik (virtio-gpu, double buffering, vsync)
- [ ] **Faz 3** — Pencere yönetimi + compositor
- [ ] **Faz 4** — UI widget toolkit
- [ ] **Faz 5** — Font & text rendering
- [ ] **Faz 6** — Görsel formatları (BMP/PNG/JPEG)
- [ ] **Faz 7** — Ses (virtio-sound)
- [ ] **Faz 8** — Ağ (virtio-net, TCP/IP, DNS, HTTP)
- [ ] **Faz 9** — Uygulama ekosistemi
- [ ] **Faz 10-13** — FS gelişmiş, güvenlik, boot/init, debug araçları

Faz 1 durum raporu: `docs/FAZ-1-DURUM.md`

## Lisans

MIT

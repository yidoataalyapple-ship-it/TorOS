# Faz 1 Durum Raporu — Input Devices

**Sürüm:** 0.1.0-faz1
**Durum:** ✅ Tamamlandı (QEMU'da doğrulandı, 11/11 otomatik test geçiyor)

## Plan ↔ İmplementasyon Eşlemesi

### 1.1 Klavye Sürücüsü

| Plan maddesi | Durum | Not |
|---|---|---|
| virtio-input PCI device tespiti | ✅ | ECAM taraması, `1AF4:1052` (modern) / `1012` (legacy) |
| BAR0 config space erişimi | ✅ | Modern: vendor capability (common/device cfg); Legacy: I/O BAR0 |
| Virtqueue eventq/statusq kurulumu | ✅ | Modern ayrık desc/driver/device + legacy PFN düzeni |
| EV_KEY → ASCII çevrimi | ✅ | `input/keymap.c` US QWERTY normal+shift tabloları |
| Modifier tracking (Shift/Ctrl/Alt/Caps) | ✅ | `keyboard_modifiers()`, Caps toggle, Ctrl kombinasyonları |
| `keyboard_getchar()` blocking | ✅ | 256-slot char ring + WFE/SEV uyandırma |
| `keyboard_poll()` non-blocking | ✅ | |
| Interrupt-driven event okuma | ✅ | GICv3 SPI (INTID 35+), INTx level |
| 64-event ring buffer | ✅ | `INPUT_EVENT_RING_SIZE` |

### 1.2 Fare Sürücüsü

| Plan maddesi | Durum | Not |
|---|---|---|
| virtio-mouse-pci tespiti | ✅ | EV_REL + BTN_LEFT bitmap analiziyle sınıflandırma |
| EV_REL delta → mutlak konum | ✅ | 1024×768 clamp (plan: framebuffer çözünürlüğü) |
| Buton durumları (L/R/M) | ✅ | bitmap |
| Wheel | ✅ | `mouse_wheel_delta()` |
| `mouse_get_state(x, y, buttons)` | ✅ | plan API'si ile birebir |

### 1.3 Touchpad

| Plan maddesi | Durum | Not |
|---|---|---|
| EV_ABS desteği | 🔶 | Altyapı hazır (`absbit` alanı, ABS_INFO config sorgusu tanımlı); QEMU virtio-tablet testi Faz 2'de GUI ile birlikte |

### 1.4 Birleşik Input Event Katmanı

| Plan maddesi | Durum | Not |
|---|---|---|
| `struct input_event` (time_sec, time_usec, type, code, value) | ✅ | plan formatı ile birebir |
| `/dev/input/eventN` kaydı | ✅ | torFS sanal aygıt dosyaları |
| Blocking + non-blocking okuma | ✅ | `input_read_event` / `input_read_event_wait` / `input_read_any` |
| Zaman damgası | ✅ | timer tick tabanlı (100 Hz) |

## Ek Olarak Yazılan Temel Bileşenler

Plan "mevcut" sayıyordu ancak kaynak kod mevcut olmadığı için hepsi sıfırdan
yazıldı:

- **Boot:** ARM64 Image header, EL2→EL1, SMP park, BSS, stack
- **MMU:** 4 seviye, 48-bit VA, 1GB blok identity map
- **GICv3:** GICD + GICR + ICC (system register) tam kurulum
- **Scheduler:** trap-frame tabanlı preemptive round-robin, sleep/yield
- **PCI:** ECAM + **BAR allocation** (firmware'siz direct boot'ta kritik) +
  QEMU GPEX INTx swizzle hesabı
- **virtio:** modern 1.0 (capability'ler, FEATURES_OK pazarlığı, VERSION_1)
  + legacy 0.9.x çift transport
- **torFS:** 64×4KB dosya + aygıt dosyası callback'leri
- **kmalloc:** serbest listeli heap + 4KB DMA sayfa allocator

## Bilinen Sınırlar / Notlar

1. **SMP:** Şu an tek çekirdek aktif (diğer 3 çekirdek park). Timer/GICR
   per-CPU kurulumu CPU0 için; SMP aktifleştirme Faz 2'de.
2. **Polling yedeği:** IRQ yanında timer tick'inden yedek tarama kancası
   mevcut (`virtio_input_poll_all`), varsayılan olarak IRQ yeterli.
3. **MSI-X:** Kullanılmıyor; INTx yeterli (QEMU virt INTx → SPI 3-6).
4. **Touchpad:** virtio-tablet desteği iskelet halinde, EV_ABS işleme Faz 2.

## Test Kanıtı

`scripts/smoke_test.py` ile otomatik doğrulama:

```
>>> Boot ve shell prompt: PASS
>>> MMU aktif log'u: PASS
>>> GICv3 aktif log'u: PASS
>>> PCI enumerasyon: PASS
>>> virtio-input klavye bulundu: PASS
>>> virtio-input fare bulundu: PASS
>>> /dev/input/event0 kayıtlı: PASS
>>> Klavye sendkey -> 'help' komutu çalıştı: PASS
>>> Fare mouse_move -> koordinat güncellendi: PASS
>>> torFS 'ls' + /dev/input/event listeleme: PASS
>>> 'events 2' komutu EV_KEY olaylarını yakaladı: PASS
SONUÇ: Tüm testler geçti ✔
```

## Sıradaki Adım: Faz 2 (Grafik)

- virtio-gpu (2D) sürücüsü: modern transport hazır, aynı pci/virtio katmanı
  üzerine `VIRTIO_GPU_CMD_*` komutları
- Framebuffer 1024×768@32bpp üzerinde double buffering + vsync
- `fb_flush()` vsync-tearing olmadan (plan: virtio-gpu RESOURCE_FLUSH)

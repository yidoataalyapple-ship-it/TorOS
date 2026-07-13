#!/usr/bin/env python3
"""
TorOS otomatik smoke testi.

QEMU'yu monitor + serial soketleriyle başlatır:
  1) Boot log'unun ve shell prompt'unun geldiğini doğrular
  2) Monitor üzerinden sendkey ile "help\n" gönderir, yardım çıkışını bekler
  3) Monitor üzerinden mouse_move gönderir, shell'den 'mouse' komutuyla
     koordinat değişimini doğrular
  4) 'ls' komutuyla torFS + /dev/input/eventN dosyalarını doğrular

Kullanım:
  python3 scripts/smoke_test.py --qemu qemu-system-aarch64 --kernel kernel.elf
"""
import argparse
import socket
import subprocess
import sys
import time
import os
import tempfile

SERIAL_TIMEOUT = 60


class QemuSession:
    def __init__(self, qemu, kernel, extra_args=None):
        self.tmp = tempfile.mkdtemp(prefix="toros-test-")
        self.serial_path = os.path.join(self.tmp, "serial.sock")
        self.monitor_path = os.path.join(self.tmp, "monitor.sock")
        args = [
            qemu,
            "-machine", "virt,gic-version=3",
            "-cpu", "cortex-a72",
            "-smp", "4",
            "-m", "2048",
            "-display", "none",
            "-kernel", kernel,
            "-device", "virtio-keyboard-pci",
            "-device", "virtio-mouse-pci",
            "-serial", f"unix:{self.serial_path},server,nowait",
            "-monitor", f"unix:{self.monitor_path},server,nowait",
            "-S",   # CPU donmuş başlar: hiçbir boot çıktısı kaçmaz
        ]
        if extra_args:
            args += extra_args
        self.proc = subprocess.Popen(args, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
        self.serial = None
        self.monitor = None
        self.buf = b""

    def connect(self):
        deadline = time.time() + 20
        while time.time() < deadline:
            try:
                self.serial = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.serial.connect(self.serial_path)
                self.serial.settimeout(0.2)
                self.monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.monitor.connect(self.monitor_path)
                self.monitor.settimeout(2.0)
                # monitor banner'ını tüket
                time.sleep(0.3)
                try:
                    self.monitor.recv(4096)
                except socket.timeout:
                    pass
                return True
            except (ConnectionRefusedError, FileNotFoundError):
                time.sleep(0.5)
        return False

    def read_serial(self, duration):
        """duration saniye boyunca serial çıktıyı topla"""
        end = time.time() + duration
        while time.time() < end:
            try:
                data = self.serial.recv(4096)
                if data:
                    self.buf += data
                    sys.stdout.write(data.decode("utf-8", "replace"))
                    sys.stdout.flush()
            except socket.timeout:
                pass
            if self.proc.poll() is not None:
                break

    def wait_for(self, pattern, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if pattern.encode() in self.buf:
                return True
            self.read_serial(0.3)
        return pattern.encode() in self.buf

    def monitor_cmd(self, cmd):
        self.monitor.sendall(cmd.encode() + b"\n")
        time.sleep(0.15)
        try:
            while True:
                data = self.monitor.recv(4096)
                if not data:
                    break
        except socket.timeout:
            pass

    def sendkey_str(self, s):
        """String'i sendkey komutlarına çevir (US layout)"""
        SHIFT_MAP = {
            '!': 'shift-1', '@': 'shift-2', '#': 'shift-3', '$': 'shift-4',
            '%': 'shift-5', '^': 'shift-6', '&': 'shift-7', '*': 'shift-8',
            '(': 'shift-9', ')': 'shift-0', '_': 'shift-minus',
            '+': 'shift-equal', '{': 'shift-bracket_left',
            '}': 'shift-bracket_right', ':': 'shift-semicolon',
            '"': "shift-apostrophe", '~': 'shift-grave_accent',
            '|': 'shift-backslash', '<': 'shift-comma', '>': 'shift-dot',
            '?': 'shift-slash',
        }
        CHAR_MAP = {
            '-': 'minus', '=': 'equal', '[': 'bracket_left',
            ']': 'bracket_right', ';': 'semicolon', "'": 'apostrophe',
            '`': 'grave_accent', '\\': 'backslash', ',': 'comma',
            '.': 'dot', '/': 'slash', ' ': 'spc', '\n': 'ret',
            '\t': 'tab',
        }
        for ch in s:
            if ch.isalpha() and ch.isupper():
                self.monitor_cmd(f"sendkey shift-{ch.lower()}")
            elif ch.isalnum():
                self.monitor_cmd(f"sendkey {ch}")
            elif ch in SHIFT_MAP:
                self.monitor_cmd(f"sendkey {SHIFT_MAP[ch]}")
            elif ch in CHAR_MAP:
                self.monitor_cmd(f"sendkey {CHAR_MAP[ch]}")
            time.sleep(0.05)

    def stop(self):
        try:
            self.monitor_cmd("quit")
        except Exception:
            pass
        time.sleep(0.3)
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qemu", default="qemu-system-aarch64")
    ap.add_argument("--kernel", default="kernel.elf")
    args = ap.parse_args()

    print("=" * 60)
    print("TorOS smoke test başlıyor")
    print("=" * 60)

    q = QemuSession(args.qemu, args.kernel)
    if not q.connect():
        print("HATA: QEMU soketlerine bağlanılamadı")
        q.stop()
        return 1
    q.monitor_cmd("cont")   # donmuş CPU'yu başlat

    failures = []

    def check(name, ok):
        status = "PASS" if ok else "FAIL"
        print(f"\n>>> {name}: {status}")
        if not ok:
            failures.append(name)

    # 1) Boot + shell prompt (ANSI kodları araya girebildiği için parçalı kontrol)
    check("Boot ve shell prompt",
          q.wait_for("toros", SERIAL_TIMEOUT) and q.wait_for("> ", 10))
    check("MMU aktif log'u", "MMU aktif" in q.buf.decode("utf-8", "replace"))
    check("GICv3 aktif log'u", "GICv3 aktif" in q.buf.decode("utf-8", "replace"))
    check("PCI enumerasyon", "PCI enumerasyon" in q.buf.decode("utf-8", "replace"))
    check("virtio-input klavye bulundu",
          "KLAVYE" in q.buf.decode("utf-8", "replace"))
    check("virtio-input fare bulundu",
          "FARE" in q.buf.decode("utf-8", "replace"))
    check("/dev/input/event0 kayıtlı",
          "devfs: /dev/input/event0" in q.buf.decode("utf-8", "replace"))

    # 2) Klavye: "help\n" gönder
    q.buf = b""
    q.sendkey_str("help\n")
    check("Klavye sendkey -> 'help' komutu çalıştı",
          q.wait_for("Komutlar:", 15))

    # 3) Fare: mouse_move + shell 'mouse' komutu
    q.buf = b""
    q.monitor_cmd("mouse_move 100 50")
    q.monitor_cmd("mouse_move 100 50")
    time.sleep(0.5)
    q.sendkey_str("mouse\n")
    ok = q.wait_for("Fare: x=", 15)
    out = q.buf.decode("utf-8", "replace")
    check("Fare mouse_move -> koordinat güncellendi",
          ok and "x=512" not in out)  # 512 başlangıç değeri

    # 4) torFS: ls komutu
    q.buf = b""
    q.sendkey_str("ls\n")
    ok = q.wait_for("dosya", 15)
    out = q.buf.decode("utf-8", "replace")
    check("torFS 'ls' + /dev/input/event listeleme",
          ok and "/dev/input/event0" in out and "/etc/motd" in out)

    # 5) events komutuyla canlı event okuma
    q.buf = b""
    q.sendkey_str("events 2\n")
    time.sleep(0.5)
    q.sendkey_str("ab")
    check("'events 2' komutu EV_KEY olaylarını yakaladı",
          q.wait_for("EV_KEY", 15) or q.wait_for("KEY", 15))

    q.stop()

    print("\n" + "=" * 60)
    if failures:
        print(f"SONUÇ: {len(failures)} test başarısız: {failures}")
        return 1
    print("SONUÇ: Tüm testler geçti ✔")
    return 0


if __name__ == "__main__":
    sys.exit(main())

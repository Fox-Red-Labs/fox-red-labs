#!/bin/sh
# E242, on the board, as root: write image D into SRAM, start core 1 with PSCI CPU_ON, read the ring.
# Usage: load_cpu1.sh zephyr.bin [entry]    (entry defaults to the module's: image D's ELF entry 0x707F10BC)
# Needs: python3, cpu1_start.ko and ringcat.py in this folder (no /tmp fallback: a root script must not load a module others can write), Linux booted with maxcpus=1.
set -eu
BIN="${1:?zephyr.bin of build-sram-core1-el1}"; ENTRY="${2:-0x707F10BC}"; D="$(dirname "$0")"
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 1; }
grep -q maxcpus=1 /proc/cmdline || echo "warning: no maxcpus=1 on the command line; the kernel may already own core 1"
# /dev/mem: the SRAM is not System RAM, so STRICT_DEVMEM (set in the SDK kernel) allows it. The mmap path is uncached Device memory, so the
# stores reach the SRAM at once; write()/dd would go through a cacheable alias and core 1 (caches off) could read stale bytes. Hence 8-byte aligned stores.
python3 - "$BIN" <<'PY'
import mmap, os, sys
img = open(sys.argv[1], "rb").read()
img += b"\0" * (-len(img) % 8)
fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
m = mmap.mmap(fd, 0x10000, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=0x707F0000)
w = memoryview(m).cast("Q"); s = memoryview(img).cast("Q")
for i in range(len(s)): w[i] = s[i]
ok = all(w[i] == s[i] for i in range(len(s)))
print(f"wrote {len(img)} bytes at 0x707F0000, readback {'ok' if ok else 'MISMATCH'}")
sys.exit(0 if ok else 1)
PY
KO="$D/cpu1_start.ko"; [ -f "$KO" ] || { echo "missing $KO"; exit 1; }
insmod "$KO" entry=$ENTRY cpu=1
dmesg | tail -1
RC="$D/ringcat.py"; [ -f "$RC" ] || { echo "missing $RC"; exit 1; }
exec python3 "$RC"

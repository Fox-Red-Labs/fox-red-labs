#!/bin/sh
# On the board (host), kernel 6.18.38-ti-kvm: boot an Alpine aarch64 "virt" kernel under QEMU with KVM, with a
# tiny initramfs (static busybox + a static CoreMark) whose init prints a mark, runs CoreMark and powers off.
# The same CoreMark binary also runs on the host, so guest and host compare like for like.
# Usage: sh vm_coremark.sh   -> /root/frl/kvm/out/
set -eu
W=/root/frl/kvm; mkdir -p $W/out
docker rm -f frl-kvm >/dev/null 2>&1 || true
docker run -d --init --name frl-kvm --device /dev/kvm -v $W:/w debian:trixie sleep infinity >/dev/null
docker exec frl-kvm sh -c 'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
  qemu-system-arm gcc libc6-dev make git ca-certificates curl cpio busybox-static >/w/out/apt.log 2>&1; qemu-system-aarch64 --version | head -1; gcc -dumpfullversion'
docker exec frl-kvm sh -c 'set -e; cd /w
  [ -d coremark ] || git clone -q --depth 1 https://github.com/eembc/coremark
  cd coremark && make -s clean >/dev/null 2>&1 || true
  make -s compile PORT_DIR=linux XCFLAGS="-O2 -static -DPERFORMANCE_RUN=1" ITERATIONS=0 >/w/out/coremark-build.log 2>&1
  cp coremark.exe /w/coremark-static; ls -l /w/coremark-static
  cd /w; [ -s vmlinuz-virt ] || curl -fsSL -o vmlinuz-virt https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/netboot/vmlinuz-virt
  rm -rf ir && mkdir -p ir/bin ir/proc ir/sys ir/dev && cp /bin/busybox ir/bin/ && cp coremark-static ir/bin/coremark
  cat > ir/init <<X
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc; mount -t sysfs sys /sys
echo "FRL-GUEST-UP \$(cut -d" " -f1 /proc/uptime) s"; cat /proc/cpuinfo | grep -m1 -iE "CPU part"; nproc
/bin/coremark 0x0 0x0 0x66 0 7 1 2000 2>&1 | grep -E "Iterations/Sec|Total time|CoreMark 1.0"
echo FRL-GUEST-DONE; poweroff -f
X
  chmod +x ir/init; (cd ir && find . | cpio -o -H newc 2>/dev/null | gzip) > initrd.gz; ls -l vmlinuz-virt initrd.gz'
echo "== host CoreMark (same binary)"; /root/frl/kvm/coremark-static 0x0 0x0 0x66 0 7 1 2000 | grep -E "Iterations/Sec|Total time"
echo "== guest"; s=$(date +%s.%N)
docker exec frl-kvm sh -c 'cd /w && timeout 600 qemu-system-aarch64 -machine virt,gic-version=2 -accel kvm -cpu host -smp 2 -m 512 \
  -kernel vmlinuz-virt -initrd initrd.gz -append "console=ttyAMA0 quiet" -nographic -no-reboot -nic none' > $W/out/guest-console.log 2>&1 || true
e=$(date +%s.%N)
grep -aE "FRL-GUEST|Iterations/Sec|Total time|CPU part|^[0-9]+\r?$" $W/out/guest-console.log || true
awk -v s=$s -v e=$e 'BEGIN{printf "qemu start to exit: %.1f s\n", e-s}'

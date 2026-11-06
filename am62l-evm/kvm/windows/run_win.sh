#!/bin/bash
# Windows 11 (22H2/23H2 arm64) as a KVM guest on the AM62L. Runs inside the frl-kvm container
# (Debian trixie, QEMU 10.0.13, qemu-efi-aarch64, swtpm), with /root/frl/kvm on the board at /w.
# Usage: run_win.sh [install|boot]   MEM (MiB, default 1600), SMP (default 2), TPM (1 or 0),
#   NET (user: virtio-net with QEMU's user-mode NAT through the board's own network, or none),
#   CACHE (qcow2 cache mode, default unsafe: no flushes to the SD card; a host crash loses the guest disk),
#   The guest screen is a VNC server on the Unix socket $W/vnc.sock only (no TCP port, nothing on the LAN);
#   a viewer on the board reaches it through that socket (vnc_panel.sh) or over an ssh tunnel.
set -e
W=/w/win
MODE=${1:-install}
MEM=${MEM:-1600}; SMP=${SMP:-2}; TPM=${TPM:-1}; NET=${NET:-user}; CACHE=${CACHE:-unsafe}
mkdir -p $W/tpm $W/unattend; chmod 700 $W   # the VNC and monitor sockets live here: root only
[ -f $W/vars.fd ] || cp /usr/share/AAVMF/AAVMF_VARS.fd $W/vars.fd
[ -f $W/disk.qcow2 ] || qemu-img create -f qcow2 $W/disk.qcow2 40G
cp -f $W/autounattend.xml $W/unattend/autounattend.xml 2>/dev/null || true
TPMARGS=()
if [ "$TPM" = 1 ]; then
  swtpm socket --tpm2 --tpmstate dir=$W/tpm --ctrl type=unixio,path=$W/tpm/sock --daemon --log file=$W/tpm/swtpm.log
  TPMARGS=(-chardev socket,id=chrtpm,path=$W/tpm/sock -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis-device,tpmdev=tpm0)
fi
CD=()
if [ "$MODE" = install ]; then
  CD=(-drive if=none,id=cd,media=cdrom,readonly=on,file=$W/win.iso -device usb-storage,drive=cd,bootindex=0
      -drive if=none,id=un,format=raw,readonly=on,file=fat:$W/unattend -device usb-storage,drive=un)
fi
NETARGS=(-nic none); [ "$NET" = user ] && NETARGS=(-netdev user,id=n0 -device virtio-net-pci,netdev=n0,mac=52:54:00:xx:xx:xx,romfile=)   # no iPXE option ROM: Windows brings its own driver
echo "start $(date +%s.%N) mode=$MODE mem=$MEM smp=$SMP tpm=$TPM net=$NET cache=$CACHE"
exec qemu-system-aarch64 -machine virt,gic-version=2 -accel kvm -cpu host -smp $SMP -m $MEM \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/AAVMF/AAVMF_CODE.no-secboot.fd \
  -drive if=pflash,format=raw,file=$W/vars.fd \
  -device ramfb -device qemu-xhci -device usb-kbd -device usb-tablet \
  "${CD[@]}" \
  -drive if=none,id=hd,format=qcow2,cache=$CACHE,file=$W/disk.qcow2 -device nvme,drive=hd,serial=frl0001,bootindex=1 \
  "${TPMARGS[@]}" "${NETARGS[@]}" -display none -vnc unix:$W/vnc.sock -serial file:$W/serial.log \
  -monitor unix:$W/mon.sock,server,nowait

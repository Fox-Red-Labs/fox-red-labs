#!/bin/bash
# Stage 1 of the install on the build host (macOS on Apple silicon, Homebrew QEMU, TCG emulation): WinPE, Setup and
# the image apply, up to Setup's first restart (-no-reboot makes QEMU exit there). The disk then goes to the
# board, where specialize, device setup and OOBE run under KVM (run_win.sh boot).
# Usage: bash stage1_build_host.sh   (in the folder with win-virtio.iso from reiso_virtio.sh)
set -e
# HVF (-accel hvf -cpu host) on an Apple M4 hangs in the Windows boot manager of build 22621 (100 percent of
# one core, no output after 12 min), so this stage emulates a Cortex-A76 with TCG instead.
Q=/opt/homebrew/share/qemu
rm -f disk.qcow2 vars.fd; qemu-img create -f qcow2 disk.qcow2 40G >/dev/null
cp AAVMF_VARS.fd vars.fd
echo "start $(date +%s)"
qemu-system-aarch64 -machine virt,highmem=on ${ACCEL:--accel hvf -cpu host} -smp 4 -m 4096 \
  -drive if=pflash,format=raw,readonly=on,file=AAVMF_CODE.no-secboot.fd -drive if=pflash,format=raw,file=vars.fd \
  -device ramfb -device qemu-xhci -device usb-kbd -device usb-tablet \
  -drive if=none,id=cd,media=cdrom,readonly=on,file=win-virtio.iso -device usb-storage,drive=cd,bootindex=0 \
  -drive if=none,id=hd,format=qcow2,cache=unsafe,file=disk.qcow2 -device nvme,drive=hd,serial=frl0001,bootindex=1 \
  -nic user,model=virtio-net-pci -display none -no-reboot -monitor unix:mon.sock,server,nowait
echo "end $(date +%s)"

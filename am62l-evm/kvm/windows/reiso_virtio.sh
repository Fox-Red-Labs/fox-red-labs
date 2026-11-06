#!/bin/bash
# Remake the install ISO with the current autounattend.xml and the virtio-win arm64 drivers in a
# $WinPEDriver$ folder, which Setup loads in WinPE and adds to the installed system.
# Build host, Debian trixie container:  docker run --rm -v $PWD:/w debian:trixie bash /w/reiso_virtio.sh
# /w holds 22621.1_PROFESSIONAL_ARM64_EN-US.ISO (build_iso.sh), autounattend.xml and virtio-win.iso
# (https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/virtio-win.iso).
set -e
cd /w
apt-get update -qq && apt-get install -y -qq 7zip genisoimage >/dev/null 2>&1
rm -rf X V && mkdir X V
7z x -oX 22621.1_PROFESSIONAL_ARM64_EN-US.ISO >/dev/null
7z x -oV virtio-win.iso 'NetKVM/w11/ARM64' 'viogpudo/w11/ARM64' 'Balloon/w11/ARM64' 'viostor/w11/ARM64' 'vioscsi/w11/ARM64' >/dev/null
chmod -R u+w X
D='X/$WinPEDriver$'; mkdir -p "$D"
for d in NetKVM viogpudo Balloon viostor vioscsi; do mkdir -p "$D/$d"; cp V/$d/w11/ARM64/*.inf V/$d/w11/ARM64/*.sys V/$d/w11/ARM64/*.cat "$D/$d/" 2>/dev/null || true; done
cp -r V/viogpudo/w11/ARM64/*.exe "$D/viogpudo/" 2>/dev/null || true
7z l virtio-win.iso | grep -m1 -i "virtio-win-" || true
find "$D" -type f | sort
cp autounattend.xml X/
genisoimage -b efi/microsoft/boot/efisys_noprompt.bin --no-emul-boot --udf -iso-level 3 --hide "*" \
  -V CCCOMA_A64FRE_EN-US_DV9 -o win-virtio.iso X 2>&1 | tail -1
rm -rf X V && sha256sum win-virtio.iso

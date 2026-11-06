#!/bin/bash
# Build a Windows 11 arm64 install ISO from Microsoft's UUP files, then remake it with the no-prompt
# EFI boot image and autounattend.xml in its root. Runs in a Debian trixie container on the build host:
#   docker run --rm -v $PWD:/w debian:trixie bash /w/build_iso.sh
# /w must hold aria2-base.txt (from uup_list.py) and autounattend.xml. About 9 GB of space.
set -e
cd /w
apt-get update -qq && apt-get install -y -qq aria2 wimtools cabextract genisoimage chntpw 7zip git ca-certificates >/dev/null
[ -d converter ] || git clone -q https://git.uupdump.net/uup-dump/converter.git
git -C converter log -1 --format="converter %H %cd"
time aria2c --no-conf --console-log-level=warn -x16 -s16 -j5 -c -R -d uup -i aria2-base.txt
time bash converter/convert.sh wim uup 0          # writes 22621.1_PROFESSIONAL_ARM64_EN-US.ISO
# The converter boots from efisys.bin, which waits for "Press any key to boot from CD or DVD";
# keys sent through the QEMU monitor did not reach it, so use efisys_noprompt.bin instead.
rm -rf X && mkdir X && 7z x -oX 22621.1_PROFESSIONAL_ARM64_EN-US.ISO >/dev/null   # UDF only: xorriso sees nothing
chmod -R u+w X && cp autounattend.xml X/
genisoimage -b efi/microsoft/boot/efisys_noprompt.bin --no-emul-boot --udf -iso-level 3 --hide "*" \
  -V CCCOMA_A64FRE_EN-US_DV9 -o win-noprompt.iso X
rm -rf X && sha256sum win-noprompt.iso

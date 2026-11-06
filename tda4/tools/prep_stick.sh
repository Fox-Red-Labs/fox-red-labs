#!/bin/bash
# Format a USB stick FAT32 (MBR) and copy the non-zero gzip chunks of TI's SD image onto it.
# Usage: prep_stick.sh diskN   (N from `diskutil list external physical`). The stick is erased.
set -euo pipefail
D="${1:?usage: prep_stick.sh diskN}"
case "$D" in disk0|disk1|disk0s*|disk1s*) echo "refusing $D (internal)"; exit 1;; esac
STICK="${STICK:?set STICK to the mount point of the stick (the volume named TDA4 after the erase)}"
diskutil info "/dev/$D" | grep -E "Device / Media Name|Disk Size|Removable Media|Protocol"
diskutil info "/dev/$D" | grep -q "Removable Media:.*Removable" || { echo "refusing $D: not removable"; exit 1; }
diskutil eraseDisk FAT32 TDA4 MBRFormat "/dev/$D"
SRC=$PWD/out/tda4/tftp
for i in $(seq 0 30) 32 40 41 48; do i=$(printf %03d $i); cp -v "$SRC/wic.$i.gz" "$STICK"/; done
cat > "$STICK"/README.txt <<'T'
Gzip chunks of tisdk-edgeai-image-j721e-evm.wic (TI Edge AI SDK 11.00.00.08), 256 MiB of raw image each.
wic.NNN.gz covers bytes NNN*268435456 onward. Chunks 031 to 051 are all zeros except 032, 040, 041 and 048 (ext4 backup superblocks); the all-zero ones are not on this stick.
Written to the SD card from U-Boot with: load usb 0:1 ${loadaddr} wic.NNN.gz, then unzip + mmc write (or gzwrite).
T
sync; ls -la "$STICK" | tail -3; df -h "$STICK" | tail -1; diskutil eject "/dev/$D"; echo "stick ready"

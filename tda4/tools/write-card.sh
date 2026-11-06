#!/bin/bash
# Write TI's Edge AI SD image to a micro-SD card, streaming the image from a second machine over ssh.
# Usage: tools/write-card.sh diskN [image.wic.xz]      (N from `diskutil list external physical`; IMG_HOST names the ssh host that holds the image; the image path is on that host)
# Refuses the internal disk. The card's contents are destroyed.
set -euo pipefail
D="${1:?usage: write-card.sh diskN}"
case "$D" in disk0|disk1|disk1s*|disk0s*) echo "refusing $D (internal)"; exit 1;; esac
diskutil info "/dev/$D" | grep -E "Device / Media Name|Disk Size|Removable|Protocol" || { echo "no such disk"; exit 1; }
diskutil info "/dev/$D" | grep -q "Removable Media:.*Removable" || { echo "refusing $D: not removable"; exit 1; }
IMG_HOST="${IMG_HOST:-image-host}"   # ssh name of the machine that holds the image
IMG="${2:-archive/tda4/tisdk-edgeai-image-j721e-evm.wic.xz}"
diskutil unmountDisk "/dev/$D"
echo "writing to /dev/r$D at $(date +%T)"
ssh "$IMG_HOST" "cat \"$IMG\"" | /usr/local/bin/xzcat | sudo dd of="/dev/r$D" bs=4m status=progress
sync; diskutil eject "/dev/$D"; echo "done $(date +%T)"

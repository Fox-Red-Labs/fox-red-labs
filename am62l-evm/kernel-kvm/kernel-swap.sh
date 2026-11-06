#!/bin/sh
# Install or restore the KVM kernel on the AM62L EVM. Run ON THE BOARD as root, booted from the SD card.
# Usage: kernel-swap.sh backup | install <bundle-dir> | restore [backup-dir] | status
# <bundle-dir> holds Image, k3-am62l3-evm.dtb, modules-6.18.38-ti-kvm.tar.gz (from build.sh output).
# Layout (SDK 12.01 image): rootfs /dev/mmcblk1p2 has /boot/Image* , /boot/dtb/, /lib/modules/<release>.
# The old kernel files are never overwritten: the new ones get a "-kvm" name and /boot/Image and
# /boot/dtb/<dtb> are pointed at them. Restore points them back and removes the new modules.
set -eu
NEWREL=6.18.38-ti-kvm
BOOT=/boot
DTB=ti/k3-am62l3-evm.dtb               # path under /boot/dtb on the SDK 12.01 card (checked 2026-10-25)
DTBN=$(basename "$DTB")               # the file name in the bundle and in a backup folder
STATE=/root/kernel-kvm-state          # keeps the pointer to the latest backup
mkdir -p "$STATE"

die() { echo "ERROR: $*" >&2; exit 1; }
need_root() { [ "$(id -u)" = 0 ] || die "run as root"; }

do_backup() {
  d=/root/kernel-backup-$(date +%Y%m%d-%H%M%S)
  mkdir -p "$d"
  ls -l "$BOOT" "$BOOT/dtb" > "$d/boot-listing.txt"
  uname -r > "$d/uname-r.txt"
  # copy dereferenced regular files so the backup is self-contained
  for f in "$BOOT"/Image*; do [ -e "$f" ] && cp -L "$f" "$d/$(basename "$f")"; done
  cp -L "$BOOT/dtb/$DTB" "$d/$DTBN"
  readlink "$BOOT/Image" > "$d/Image.link" 2>/dev/null || :
  readlink "$BOOT/dtb/$DTB" > "$d/dtb.link" 2>/dev/null || :
  tar -C /lib/modules -czf "$d/modules-$(uname -r).tar.gz" "$(uname -r)"
  ( cd "$d" && sha256sum * > SHA256SUMS )
  echo "$d" > "$STATE/latest"
  echo "backup: $d"; du -sh "$d"
}

do_install() {
  b=${1:-}; [ -d "$b" ] || die "usage: install <bundle-dir>"
  [ -f "$b/Image" ] && [ -f "$b/$DTBN" ] && [ -f "$b/modules-$NEWREL.tar.gz" ] || die "bundle incomplete"
  ( cd "$b" && sha256sum -c SHA256SUMS ) || die "bundle checksum mismatch"
  [ -f "$STATE/latest" ] || die "run '$0 backup' first"
  cp "$b/Image" "$BOOT/Image-$NEWREL"
  cp "$b/$DTBN" "$BOOT/dtb/${DTB%.dtb}-kvm.dtb"
  tar -C /lib/modules -xzf "$b/modules-$NEWREL.tar.gz"
  # repoint (a regular-file Image was already copied to the backup dir)
  ln -sf "Image-$NEWREL" "$BOOT/Image.new" && mv -f "$BOOT/Image.new" "$BOOT/Image"
  ln -sf "${DTBN%.dtb}-kvm.dtb" "$BOOT/dtb/$DTB.new" && mv -f "$BOOT/dtb/$DTB.new" "$BOOT/dtb/$DTB"
  sync
  echo "installed $NEWREL. Reboot to try it. To go back: $0 restore"
}

do_restore() {
  d=${1:-$(cat "$STATE/latest" 2>/dev/null)}; [ -d "${d:-}" ] || die "no backup dir (give one)"
  ( cd "$d" && sha256sum -c SHA256SUMS ) || die "backup checksum mismatch"
  old=$(cat "$d/uname-r.txt")
  # Image
  if [ -s "$d/Image.link" ]; then ln -sf "$(cat "$d/Image.link")" "$BOOT/Image.new" && mv -f "$BOOT/Image.new" "$BOOT/Image"
  else cp "$d/Image" "$BOOT/Image"; fi
  # dtb
  if [ -s "$d/dtb.link" ]; then ln -sf "$(cat "$d/dtb.link")" "$BOOT/dtb/$DTB.new" && mv -f "$BOOT/dtb/$DTB.new" "$BOOT/dtb/$DTB"
  else cp "$d/$DTBN" "$BOOT/dtb/$DTB"; fi
  [ -d "/lib/modules/$old" ] || tar -C /lib/modules -xzf "$d/modules-$old.tar.gz"
  rm -rf "/lib/modules/$NEWREL" "$BOOT/Image-$NEWREL" "$BOOT/dtb/${DTB%.dtb}-kvm.dtb"
  sync
  echo "restored $old from $d. Reboot."
}

case "${1:-status}" in
  backup)  need_root; do_backup ;;
  install) need_root; shift; do_install "$@" ;;
  restore) need_root; shift; do_restore "$@" ;;
  status)  uname -r; ls -l "$BOOT"/Image* "$BOOT/dtb/$DTB"; cat "$STATE/latest" 2>/dev/null || echo "no backup yet" ;;
  *) die "usage: $0 backup | install <bundle-dir> | restore [backup-dir] | status" ;;
esac

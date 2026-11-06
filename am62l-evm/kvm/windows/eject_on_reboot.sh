#!/bin/sh
# eject_on_reboot.sh: on the board host. Polls the guest's program counters every 2 s; when both vCPUs
# run below 4 GB (UEFI firmware or the boot manager, not the Windows kernel), the guest has rebooted
# out of Setup, so the install ISO is ejected before the CD can boot Setup again and wipe the disk.
D=/root/frl/kvm/win; T0=$(cut -d. -f1 $D/t0)
mon() { echo "$1" | docker exec -i frl-kvm socat - UNIX-CONNECT:/w/win/mon.sock; }
while pgrep -f qemu-system-aarch64 >/dev/null; do
  N=$(mon "info registers -a" | grep -o 'PC=[0-9a-f]*' | awk -F= '{ if (length($2)==16 && substr($2,1,8)=="00000000") n++ } END {print n+0}')
  if [ "$N" -ge 2 ]; then
    mon "eject -f cd" >/dev/null; echo "$(( $(date +%s)-T0 )) reboot seen, cd ejected" >> $D/watch.log
    mon "info block" | grep '^cd' >> $D/watch.log; exit 0
  fi
  sleep 2
done

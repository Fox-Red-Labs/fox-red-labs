#!/bin/sh
# On the board host: stop any earlier helpers and guest, then start a fresh install (new disk, firmware vars
# and TPM state) with the ISO eject helper and the 30 s screenshot logger beside it.
# Usage: sh start_install.sh [install|boot]
D=/root/frl/kvm/win; M=${1:-install}
for p in $(ps -eo pid,args | awk '/[e]ject_on_reboot.sh|[w]atch.sh 12000/{print $1}'); do kill $p; done
echo quit | docker exec -i frl-kvm socat - UNIX-CONNECT:/w/win/mon.sock >/dev/null 2>&1; sleep 2
docker exec frl-kvm sh -c 'kill $(pidof swtpm qemu-system-aarch64) 2>/dev/null'; sleep 1
cd $D
if [ "$M" = install ]; then rm -rf tpm disk.qcow2 vars.fd watch.log; fi
docker exec -d frl-kvm bash -c "cd /w/win && bash run_win.sh $M > qemu-$M.log 2>&1"
date +%s.%N > t0; sleep 8
# The eject helper treats "both vCPUs in firmware" as Setup's restart, which is also true during the first
# boot, so it starts only once WinPE is up (80 s in E287).
[ "$M" = install ] && { (sleep 300; sh eject_on_reboot.sh) >/dev/null 2>&1 & }
nohup sh watch.sh 12000 30 >/dev/null 2>&1 &
cat qemu-$M.log

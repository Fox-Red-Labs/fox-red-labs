# How do you run Windows 11 arm64 as a KVM guest on the TI TMDS62LEVM (AM62L)?

Could work: Windows 11 Pro 22H2 (build 22621.1) installs unattended and boots twice from its own disk, then stops in the out-of-box experience on "Why did my PC restart?" at 7,231 s. Write-up: `docs/experiments/2026-10-25-am62l-windows-arm-vm.md`. Needs the `6.18.38-ti-kvm` kernel ([kernel-kvm](../../kernel-kvm/README.md)). Terms: [docs/vocab.md](../../../docs/vocab.md).

| File | Runs on | What it does |
|---|---|---|
| `uup_list.py` | build host | Turns the UUP dump API file list (Microsoft's URLs and SHA-1s) into an `aria2c` input file, without the cumulative updates |
| `build_iso.sh` | build host, Debian trixie container | Downloads, converts to an ISO, remakes it with `efisys_noprompt.bin` and `autounattend.xml` |
| `autounattend.xml` | in the ISO | LabConfig bypass keys, disk 0 wiped and partitioned, empty product key, built-in Administrator auto-logon, no online account |
| `run_win.sh` | board, `frl-kvm` container | Starts swtpm and QEMU (`install` with the ISO, `boot` without); `MEM`, `SMP`, `TPM` override the defaults |
| `shot.sh` | board, `frl-kvm` container | Screendump of the guest display, or one key through the monitor |
| `watch.sh` | board host | Screenshot and QEMU CPU, memory and disk counters every N seconds |
| `eject_on_reboot.sh` | board host | Ejects the ISO when both vCPUs return to firmware after Setup's first restart |

```sh
# build host
curl -fsS "https://api.uupdump.net/get.php?id=<uuid>&lang=en-us&edition=professional" -o uup.json
python3 -I uup_list.py uup.json aria2-base.txt
docker run --rm -v $PWD:/w debian:trixie bash /w/build_iso.sh
# board: ISO as /root/frl/kvm/win/win.iso, scripts beside it
fallocate -l 2G /swapfile-win && chmod 600 /swapfile-win && mkswap /swapfile-win && swapon /swapfile-win
docker exec frl-kvm bash -c "apt-get install -y qemu-efi-aarch64 swtpm qemu-utils socat"
docker exec -d frl-kvm bash -c "cd /w/win && bash run_win.sh install > qemu.log 2>&1"
date +%s.%N > /root/frl/kvm/win/t0
sh eject_on_reboot.sh & sh watch.sh 9000 20 &
```

Stop with `echo quit | docker exec -i frl-kvm socat - UNIX-CONNECT:/w/win/mon.sock`. Stop the Doom demo and the Steam container first: the guest needs about 1.6 GB.

# How do you build and boot a 6.18.38 kernel with KVM and binder on the TI TMDS62LEVM (AM62L)?

Works. The custom kernel `6.18.38-ti-kvm` boots the TMDS62LEVM from its micro-SD card (2026-10-25). KVM starts in nVHE mode, binderfs mounts, and Redroid 12 (Android in a container) reaches `sys.boot_completed=1` in 86 s. This folder builds the kernel. It adds what TI's SDK 12.01 kernel lacks: `CONFIG_KVM` (the CPUs already start at EL2), Android binder and binderfs, `binfmt_misc`, and the PlayStation drivers. The fragment is `kvm-binder.config`; everything else comes from the board's running `/proc/config.gz`. Results and steps: `docs/experiments/2026-10-25-am62l-kvm-binder-kernel.md`.

Terms: [docs/vocab.md](../../docs/vocab.md). **KVM** is Linux's hypervisor; **nVHE** is its mode for cores without the ARMv8.1 Virtualization Host Extensions (the A53 is ARMv8.0); **binder** is Android's IPC driver; **binderfs** creates its devices; **vermagic** is the version string a module must match to load. 1 kB = 1,024 bytes.

## Source

TI kernel `ti-linux-kernel`, branch `ti-linux-6.18.y-cicd`, tag `cicd.wrynose.202607100005`, commit `8933766be737` (merge of v6.18.38). It is the tree in the SDK (`board-support/ti-linux-kernel-6.18.38+git-ti`) and matches the commit in the board's `uname -r` (`6.18.38-ti-00934-g8933766be737-dirty`). The SDK checkout shows 15 files "modified" because APFS ignores case (`xt_CONNMARK.h` and `xt_connmark.h` collide), so the build uses `git archive HEAD` on a case-sensitive volume.

## Config changes (`kvm-binder.config`)

| Option | Was | Now |
|---|---|---|
| `VIRTUALIZATION` / `KVM` | `y` / not set | `y` / `y` (plus vhost-net, vhost-vsock, vsock, tun, bridge, veth as modules) |
| `ANDROID_BINDER_IPC`, `ANDROID_BINDERFS` | not set | `y` (devices `binder,hwbinder,vndbinder`; ashmem no longer exists upstream, Android uses memfd), `PSI=y` |
| `BINFMT_MISC` | not set | `y` |
| `USER_NS` and the other namespaces, `SECCOMP_FILTER` | already `y` on the board | kept `y` |
| `HID_PLAYSTATION`, `HID_SONY`, `INPUT_UINPUT` | not set | `m` |
| `LOCALVERSION` | empty in the config; the SDK build adds `-ti` and the git hash from its environment | `-ti-kvm`, so release is `6.18.38-ti-kvm` and the stock modules stay untouched |

`merge_config.sh` prints any option that did not take effect; `final.config` in the build folder is the result.

## Build (any arm64 Linux or Docker host)

The build needs a case-sensitive volume of about 40 GB. On a Mac, make a case-sensitive sparsebundle, mount it with `hdiutil attach`, and it appears as `/Volumes/kbuild`. It holds `src/`, `running-config`, `kvm-binder.config`, `build.sh` and, after the build, `out/` (build tree), `bundle/` and `build.log`.

```bash
docker run --rm --network host --platform linux/arm64 -v /Volumes/kbuild:/kb debian:trixie bash /kb/build.sh
```

The container is native arm64, so no cross prefix. The compiler is Debian's gcc, not the SDK's 15.3.0; module vermagic only checks the release string, not the compiler. The container needs outbound TCP for `apt`. If the build host has none, download the Debian build-tool packages elsewhere, check them against the Debian index SHA256, copy them to `/Volumes/kbuild/debs`, and `build.sh` installs them with `dpkg -i`.

## Package

`/Volumes/kbuild/bundle/` after the build: `Image`, `k3-am62l3-evm.dtb`, `modules-6.18.38-ti-kvm.tar.gz`, `config-6.18.38-ti-kvm`, `kvm-binder.config`, `kernel-swap.sh`, `SHA256SUMS`.

## Install and restore

1. Image the SD card first on a Mac (`sudo dd if=/dev/rdiskN of=sd-backup.img bs=4m`), so you can write it back.
2. Copy the bundle to the board (`scp -r bundle root@<board ip>:/root/kvm-bundle`), then on the board, booted from that card:

```sh
/root/kvm-bundle/kernel-swap.sh backup                 # copies Image*, dtb, modules to /root/kernel-backup-<date>
/root/kvm-bundle/kernel-swap.sh install /root/kvm-bundle   # new files get a -kvm name; /boot/Image and the dtb are repointed
reboot
```

3. Restore, from the booted new kernel: `/root/kvm-bundle/kernel-swap.sh restore` (repoints, removes the new modules).
4. If it does not boot: stop U-Boot at `=>` on the serial console and boot the stock files by name (the new files stay on the card):

```
load mmc 1:2 ${loadaddr} /boot/Image-6.18.38-ti-00934-g8933766be737-dirty
load mmc 1:2 ${fdtaddr} /boot/dtb/ti/k3-am62l3-evm.dtb
setenv bootargs console=ttyS0,115200n8 earlycon=ns16550a,mmio32,0x02800000 root=PARTUUID=076c4a2a-02 rw rootfstype=ext4 rootwait cma=64M
booti ${loadaddr} - ${fdtaddr}
```

The stock dtb is `/boot/dtb/ti/k3-am62l3-evm.dtb` (U-Boot `fdtfile=ti/k3-am62l3-evm.dtb`); the bundle's dtb is byte-identical. These four commands booted the stock kernel by hand on 2026-10-25 (login 19.4 s after `booti`). Last resort: write `sd-backup.img` back.

## Expected after boot

```
CPU: All CPU(s) started at EL2
kvm [1]: Hyp mode initialized successfully
```

After `kernel-swap.sh install` and `reboot`, the new kernel reached login 32.8 s later. `ls -l /dev/kvm` shows a character device. `mount -t binder binder /mnt` works, `cat /proc/config.gz | zcat | grep BINFMT_MISC` shows `=y`, `modprobe hid-playstation` loads.

## Build result

`Image` is ARM64 little-endian, release `6.18.38-ti-kvm`, with 732 modules including `drivers/hid/hid-playstation.ko`. The build used Debian gcc 14.2 instead of the SDK's gcc 15.3.

| Item | Value |
|---|---|
| Build time | 380 s for the resumed run (6 jobs, 10-core host, native arm64 container); an earlier run died after about 2,750 objects when Docker restarted, so a clean build is longer (not measured) |
| Image size | 21.5 MiB (stock Image: 20.6 MiB) |
| Modules tarball | 10.9 MiB (`modules-6.18.38-ti-kvm.tar.gz`) |
| Bundle | `/Volumes/kbuild/bundle/` 33 MB, with SHA256SUMS; build tree `/Volumes/kbuild/out` 1.6 GB |
| Stock dtb name on the card | `/boot/dtb/ti/k3-am62l3-evm.dtb`; `kernel-swap.sh` uses it |

Not shown: real-time latency or power with the new kernel, a guest with a disk or network, an Android app running, and whether a cold power cycle boots the same way as `reboot`.

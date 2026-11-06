<!-- experiment
id: E285
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-25
status: works
reached: run
line: A 6.18.38 kernel built from TI's SDK tree with KVM, binder, binfmt_misc and the PlayStation drivers added boots the AM62L from its SD card. KVM starts in nVHE mode; an Alpine guest runs CoreMark at 95 to 98 percent of the host, and Redroid (Android 12 in a container) reaches boot completed in 86 s.
stop: Android booted to its launcher; no app installed or run in it
-->
# Will the AM62L boot a 6.18.38 kernel with KVM and binder?

**Short answer.** Yes. The custom kernel `6.18.38-ti-kvm` boots from the SD card to login, `dmesg` shows `kvm [1]: Hyp nVHE mode initialized successfully`, `/dev/kvm` exists, binderfs mounts with `binder`, `hwbinder` and `vndbinder`, `binfmt_misc` mounts, and `hid-playstation` loads. A KVM guest runs CoreMark at 3.9k to 4.0k iterations per second against 4.1k to 4.2k on the host with the same binary. Redroid 12 (Android in a container, on binder) reaches `sys.boot_completed=1` in 86 s and shows its launcher.

Terms: [vocab](../vocab.md). **KVM** is Linux's hypervisor; **nVHE** is its mode for cores without the ARMv8.1 Virtualization Host Extensions (the A53 is ARMv8.0). **binder** is Android's IPC driver; **binderfs** creates its devices. **Redroid** runs Android's user space in a container. Build and install: `am62l-evm/kernel-kvm/README.md`; rows in `data/results/am62l-evm.csv`.

**What changes.** The SDK 12.01 kernel (6.18.38, tag `cicd.wrynose.202607100005`, commit `8933766be`) keeps the board's running config, plus `am62l-evm/kernel-kvm/kvm-binder.config`: KVM, binder and binderfs, binfmt_misc, the PlayStation, Sony and uinput drivers. Built with Debian gcc 14.2 in a native arm64 container: `Image` 22.5 MB, 732 modules. The device tree in the bundle is byte-identical to the card's.

**Expectation (before the run).** Boots to login in about the stock time; `Hyp mode initialized` because every CPU already starts at EL2; a guest within a few percent of the host on CoreMark (KVM runs guest code natively).

**Steps and results.**

| Step | Result |
|---|---|
| Recovery path first: reboot on the stock kernel, stop U-Boot's autoboot on the console, boot stock by hand | Works: at `=>`, `load mmc 1:2 ${loadaddr} /boot/Image-6.18.38-ti-00934-g8933766be737-dirty`, `load mmc 1:2 ${fdtaddr} /boot/dtb/ti/k3-am62l3-evm.dtb`, `setenv bootargs ...`, `booti ${loadaddr} - ${fdtaddr}`; login 19.4 s after `booti` |
| `kernel-swap.sh backup` | 50 MB: both Image files, the dtb, the stock modules; copied off the board and checked against SHA256SUMS |
| `kernel-swap.sh install`, reboot (normal autoboot) | Login 32.8 s after the reboot command; last kernel line before login at 17.7 s (stock: 18.2 s) |
| KVM | `CPU: All CPU(s) started at EL2`, `kvm [1]: Hyp nVHE mode initialized successfully`, vgic-v2, IPA limit 40 bits; `/dev/kvm` present |
| binder | `mount -t binder binder /dev/binderfs` gives `binder`, `binder-control`, `hwbinder`, `vndbinder` |
| binfmt_misc, PlayStation driver | mounts; `modprobe hid-playstation` loads, and the pad then reports the standard button names |
| VM: Alpine 3.22 `virt` kernel, QEMU 10.0.13, `-accel kvm -cpu host -smp 2 -m 512`, initramfs that runs CoreMark | Guest init at 0.50 s guest uptime; QEMU start to power-off 19.9 to 20.2 s, of which CoreMark is about 15 s |
| CoreMark, same static binary (GCC 14.2 -O2), host vs guest, 3 runs each | host 4.08k, 4.08k, 4.15k; guest 4.02k, 3.93k, 3.93k iterations/s |
| Redroid `12.0.0_64only-latest` (1.97 GB image, 305 s to pull), `--privileged`, adb on loopback | `sys.boot_completed=1` at 86 s; focus `com.android.launcher3/...QuickstepLauncher`; screen 720x1280; whole-board RAM peak 989 MB during boot, 1,166 MB used after |

**Fixes made on the way.** `kernel-swap.sh` assumed `/boot/dtb/k3-am62l3-evm.dtb`; on the card it is `/boot/dtb/ti/k3-am62l3-evm.dtb` (U-Boot's `fdtfile=ti/k3-am62l3-evm.dtb`), so the script now uses that path. U-Boot loads `/boot/Image` and the dtb from the ext4 root (`bootpart=1:2`, `name_kern=Image`); the `Image` on the FAT partition is not used. QEMU needs `-nic none` when the `ipxe-qemu` ROMs are not installed (`failed to find romfile "efi-virtio.rom"`).

**What it does not show.** Real-time latency or power with the new kernel; a guest with a disk or network; an Android app running, its frame rate, or input; KVM with more than one guest; whether a cold power cycle (rather than `reboot`) boots the same way.

**Restore.** From the booted new kernel: `kernel-swap.sh restore`, then reboot. If it does not boot: stop U-Boot at `=>` and use the four commands in the first table row (proven on this card). The backup is in `/root/kernel-backup-20261025-174552` on the board and copied off it.

**If you try it.** If `kernel-swap.sh backup` stops on a missing dtb, read `fdtfile` with `printenv fdtfile` at the U-Boot prompt and point `DTB` at it. If QEMU stops on `efi-virtio.rom`, add `-nic none` or install `ipxe-qemu`. Pass `-machine virt,gic-version=2` to QEMU: the kernel reports a GICv2-style virtual interface (`vgic-v2`).

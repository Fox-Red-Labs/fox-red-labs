# TI Linux SDK 12.01.00.05.03 for the AM62L, unpacked on an arm64 Mac build host

Download, unpack and use the TI Linux SDK 12.01.00.05.03 for the AM62L (TMDS62LEVM) on an arm64 Mac. It does not run natively, so the install goes through a container (35 minutes, 7.5 GB). Installed 2026-10-22; everything lives on the build host under `/Volumes/ti/am62l-sdk/` (a 40 GB sparse volume).

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

## Download and checks

| Item | Value |
|---|---|
| Installer | `ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03-Linux-x86-Install.bin`, 4.26 GiB (matches the server's Content-Length of 4.57 billion bytes) |
| sha256 | `181ced9857f0e6ce8b11e073706d60bb7665b888558dffa92c21d47d315266fb` (equals the ETag the download server returned; no checksum is listed on the product page that was checked) |
| URL | `https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-YjEeNKJJjt/12.01.00.05.03/<file>` |
| Installed size | 7.5 GB (`du`) in `/Volumes/ti/am62l-sdk/ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03/` |
| Volume after install | 17 GB used, 23 GB free (the 4.4 GB installer is still there) |

## How it was installed

The installer is a UPX-packed x86_64 Linux ELF. On an arm64 Mac it fails under Rosetta (`bss_size overflow`), and plain `qemu-x86_64` fails because the unpacked program is dynamically linked. What worked: an arm64 `debian:trixie` container in OrbStack, `qemu-user`, and an amd64 libc extracted with `dpkg -x` (`libc6`, `libgcc-s1`, `libstdc++6`, `zlib1g` as amd64 debs) used as the qemu root, with `/x/root/lib64/ld-linux-x86-64.so.2` linked to the extracted loader:

```
qemu-x86_64 -L /x/root /work/<installer>.bin --mode unattended --prefix /work/ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03 </dev/null
```

It took about 35 minutes. Exit code 0. The installer unpacks an embedded `tisdk-core-bundle` tar.xz and deletes it.

## What is in the tree

Root: `/Volumes/ti/am62l-sdk/ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03/` (called `$SDK` below).

| Path under `$SDK` | Size | What it is |
|---|---|---|
| `board-support/ti-linux-kernel-6.18.38+git-ti/` | 1.9 GB | Kernel source, git checkout at `8933766be` (merge of v6.18.38): the commit in the board's `uname` (`6.18.38-ti-00934-g8933766be737-dirty`). The "dirty" is a handful of modified files in the working tree (for example `arch/arm/tools/gen-mach-types`), which `git status` shows |
| `board-support/linux-extras-6.18.38+git-ti/` | 1.9 GB | Second kernel checkout (also a git tree), used by the SDK for the extra-drivers builds. Not compared against the first one |
| `board-support/ti-u-boot-2026.01+git/` | 392 MB | U-Boot. AM62L configs: `am62lx_evm_defconfig`, `am62lx_beaglebadge_defconfig`, `am62lx_minimal_defconfig`, `am62lx_falcon.config` |
| `board-support/u-boot-extras-jailhouse-2026.01+git/` | 392 MB | U-Boot tree used for the Jailhouse image |
| `board-support/trusted-firmware-a-2.15+git/` | 101 MB | TF-A |
| `board-support/optee-os-4.10.0+git/` | 70 MB | OP-TEE OS |
| `board-support/extra-drivers/` | 12 MB | `cryptodev-module-1.14`, `jailhouse-0.12+git` |
| `board-support/prebuilt-images/am62lxx-evm/` | 55 MB | `tiboot3.bin`, `tispl.bin`, `u-boot.img`, `u-boot-spl.bin-am62lxx-evm`, `bl1.bin`, `bl31.bin`, `bl32.bin`, `Image`, `fitImage`, `ti-sysfw/`, `uEnv.txt`, and the device trees and overlays (`k3-am62l3-evm.dtb`, `*.dtbo`) |
| `board-support/prebuilt-images/am62lxx-evm-jailhouse/` | 55 MB | The same file set built for Jailhouse (its own `uEnv.txt`, device trees, `fitImage`) |
| `filesystem/am62lxx-evm/` | 2.6 GB (with the jailhouse folder) | Root filesystems as tar.xz: base, default and thinlinux images, plus `tisdk-tiny-initramfs-am62lxx-evm.cpio` |
| `filesystem/am62lxx-evm-jailhouse/` | | `tisdk-jailhouse-image-am62lxx-evm.rootfs.tar.xz` |
| `example-applications/arm-benchmarks-1.3/` | 180 kB | Dhrystone, Linpack, Whetstone sources. This is the only example-application folder in the installer |
| `bin/` | 364 kB | Host helper scripts: `create-sdcard.sh`, `setup-uboot-env.sh`, `setup-tftp.sh`, `setup-targetfs-nfs.sh`, `setup-minicom.sh`, `setup-snagboot.sh`, `snagboot_flash/` |
| `manifest/software_manifest.txt` | 1.9 MB | Package list, with versions |
| `Rules.make`, `Makefile`, `setup.sh` | | SDK build variables. `UBOOT_MACHINE=am62lx_evm_defconfig`; `ARCH=arm64`; the file's `TI_SDK_PATH` default is the `--prefix` given at install time (`/work/...` in the container), so edit it for use on the build host |

Kernel configs: `arch/arm64/configs/defconfig` (the only defconfig; there is no AM62L-specific one) plus fragments in `kernel/configs/` (`ti_arm64_prune.config`, `ti_rt.config`, `debug.config`, `hardening.config`, `nopm.config`, `no_smp.config`, ...).

## Not in the installer: the cross toolchain

`Rules.make` expects `$SDK/linux-devkit` (CROSS_COMPILE `aarch64-oe-linux-`) and `$SDK/k3r5-devkit`. Neither folder exists after this install, and no `aarch64` gcc was found in the tree. The manifest names the toolchain the SDK was built with: gcc 15.3.0 (`gcc-cross-canadian-aarch64`, x86_64 host). Where TI publishes that devkit as a separate download is not known. Until it is found there are two options:

1. The Debian toolchain (`gcc-aarch64-linux-gnu`, 14.2) in an amd64 or arm64 container. On an arm64 host, an arm64 container needs no cross prefix at all (`ARCH=arm64`, `CROSS_COMPILE=` empty).
2. The TI devkit installer, once you find its name.

A module must be built with the same compiler major version as the kernel for a clean `vermagic`, but only the kernel's `vermagic` string (release plus options) is checked at load time, not the compiler. The compiler version appears in `/proc/version` on the board.

## Build an out-of-tree module against the SDK kernel (not run)

Use a separate build directory (`O=`) so the SDK tree stays unmodified. Replace `CROSS` with the toolchain prefix from the section above (empty for a native arm64 container).

```bash
SDK=/Volumes/ti/am62l-sdk/ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03
K=$SDK/board-support/ti-linux-kernel-6.18.38+git-ti
B=/Volumes/ti/am62l-sdk/kbuild            # build output, outside the SDK
CROSS=aarch64-linux-gnu-                  # or the TI devkit prefix, or empty (native arm64)

# 1. Configuration: either the SDK default plus TI's fragment ...
make -C $K O=$B ARCH=arm64 CROSS_COMPILE=$CROSS defconfig
# ... or, better, the config of the running board:
#   ssh root@<board> zcat /proc/config.gz > $B/.config
#   make -C $K O=$B ARCH=arm64 CROSS_COMPILE=$CROSS olddefconfig
# (needs CONFIG_IKCONFIG_PROC on the board; check that /proc/config.gz exists)

# 2. Kernel build scripts and generated headers (no full kernel build)
make -C $K O=$B ARCH=arm64 CROSS_COMPILE=$CROSS modules_prepare

# 3. The module (Makefile: obj-m := hello.o)
make ARCH=arm64 CROSS_COMPILE=$CROSS -C $K O=$B M=/path/to/module modules
```

With `O=`, the `-C` target is the source tree and `M=` must be an absolute path. Host needs: `make flex bison bc libssl-dev libelf-dev perl python3`.

For the module to load, `vermagic` must match the board's `uname -r` (`6.18.38-ti-00934-g8933766be737-dirty`). That string comes from `CONFIG_LOCALVERSION`, the git hash and the "dirty" flag, so the board's `.config` and a matching `LOCALVERSION` are needed; `modinfo hello.ko | grep vermagic` shows what was built. If the board was built with `CONFIG_MODVERSIONS`, a `Module.symvers` from a full kernel build with the same config is also needed (`modules_prepare` does not produce it).

## Not verified

- `modules_prepare` and an example module build were attempted once in an arm64 Debian container and did not finish: the build host's Docker engine went down mid-build. Use a smaller job count (`-j2`).
- That the board's `.config` matches `defconfig`: the board's `/proc/config.gz` has not been read.
- That the SDK kernel tree equals the one the board runs: only the git hash and the "dirty" suffix agree with `uname`.
- The location of TI's separate Linux devkit download.
- Whether `linux-extras` differs from `ti-linux-kernel`.

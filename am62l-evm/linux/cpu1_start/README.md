# cpu1_start: how Linux starts Zephyr on core 1 of the TI TMDS62LEVM (AM62L)

The module is written and has not been compiled or run on the board. The working way to start core 1 is PSCI from U-Boot, in [the board README](../../README.md#zephyr-on-core-1-beside-linux). Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes.

DSLite cannot attach to CortexA53_1 while the core is powered down (error -1062), so Linux starts it: a PSCI `CPU_ON` for core 1 with the entry address of image D (`build-sram-core1-el1`, ELF entry 0x707F10BC, code in WKUP SRAM at 0x707F0000). TF-A powers the core through TI-SCI and enters the address in the non-secure world.

Files: `cpu1_start.c` (the module, `entry` and `cpu` parameters), `Makefile`, `load_cpu1.sh` (board script: writes `zephyr.bin` into SRAM, loads the module, runs `ringcat.py`). The first build is on the build host or the board.

## Core 1's MPIDR

`cpu1_start` passes MPIDR = `cpu` (Aff0 = 1, Aff1 = 0). Source: TF-A `plat/ti/k3/common/k3_topology.c`, `plat_core_pos_by_mpidr()` reads only Aff1 (cluster) and Aff0 (core), returns -1 if Aff2 or Aff3 is non-zero, and adds the earlier clusters' core counts. The core's own MPIDR_EL1 reads 0x80000001 (bit 31 is RES1, not part of the PSCI argument). Check what Linux itself uses on the board: `xxd /proc/device-tree/cpus/cpu@1/reg` (expected 00 00 00 01).

## Build way 1: against TI's SDK kernel source on the build host

Package: `ti-processor-sdk-linux-am62lxx-evm-12.01.00.05.03-Linux-x86-Install.bin`, 4.3 GiB, from https://www.ti.com/tool/download/AM62L-LINUX-SDK/12.01.00.05.03 (release 31 Jul 2026, kernel ti-linux-6.18.y, 6.18.38). It is an x86 Linux installer, so on a Mac run it in a Linux container; [SDK.md](../../SDK.md) has the steps that worked. The SDK has the kernel source under `board-support/` but no cross toolchain. The module must match the running kernel's version string and config, so use the SDK's own kernel tree, prepared with `zcat /proc/config.gz > .config` (copied from the board) then `make ARCH=arm64 CROSS_COMPILE=<sdk toolchain>- olddefconfig modules_prepare`. Then:

    make KDIR=<sdk>/board-support/<kernel tree> CROSS_COMPILE=<sdk toolchain>-

## Build way 2: on the board

Needs kernel headers that match the running kernel. Check: `ls /lib/modules/$(uname -r)/build`. If it exists and has a `Makefile`: you also need `gcc` and `make` on the board, then run `make` in this folder. If the folder is missing (the SDK Yocto image normally ships no headers), use way 1.

## Use

On the board, as root, Linux booted with `maxcpus=1`: copy `cpu1_start.ko`, `load_cpu1.sh`, `ringcat.py` and `zephyr.bin` to one folder, then `./load_cpu1.sh zephyr.bin`. Expected: `wrote 44744 bytes at 0x707F0000, readback ok`, then dmesg `PSCI CPU_ON ... -> 0`, then the ring banner with `Aff0=1`.

If the return is -9 the address is rejected; -3 TF-A denied (core already owned); -4 already on. If the call returns 0 and the ring stays empty, the core entered but faulted: check `CurrentEL` assumptions, and that TF-A's own use of WKUP SRAM at 0x707F0000 (its A53 resume stub, per the boot log) is not where the CPU_ON boot address points (TI-SCI is given TF-A's warm-boot entry, `k3_sec_entrypoint`, whose location is not stated in the K3 source read).

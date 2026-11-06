<!-- experiment
id: E273
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: in progress
reached: run
line: TI's SDK 12.01 default image was written to a new 128 GB microSD card from the running Linux and read back identical except one automount-touched megabyte; the SD boot itself waits on the SW3 change. The kernel has no PlayStation controller driver, but one built on the board loads.
stop: SD boot needs SW3 set for SD and a J17 power cycle.
-->
# Can the AM62L boot from a new SD card, and can its kernel read a DualSense?

**Short answer.** The card is detected (`mmcblk1`, 119.2 GiB, UHS-I SDR104) and holds the image. The old card failed with a voltage-select timeout, so the card was the fault. The kernel lacks `CONFIG_HID_PLAYSTATION`, `CONFIG_HID_SONY` and `CONFIG_INPUT_UINPUT`; a `hid-playstation.ko` built on the board loads. No controller was attached to it. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**SD card.** The `.wic.xz` (sha256 810bf62d... matches TI's published checksum) was decompressed on the build host and piped over ssh into `dd of=/dev/mmcblk1 bs=4M conv=fsync`: 14.5 GB in 1,943.7 s (8.0 MB/s, limited by the pipe). Read back: hashes of 59 chunks of 256 MiB are equal to the source stream except chunk 0; at 1 MiB steps only MiB 129 (the start of the ext4 partition, its superblock) differs, because udev mounted both partitions read-write after the write finished. The USB stick and the eMMC were not written. Unmounted before the check.

**Boot switches.** SD boot is the as-shipped setting, SW3 1 to 4 all OFF (now OFF ON ON ON for UART boot). Boot time and root device after the change are not recorded (`linux_boot_time`, `sd-boot-to-login`, waiting).

**PlayStation controllers.** `zcat /proc/config.gz`: `HID_PLAYSTATION` not set, `HID_SONY` not set, `UHID` not set, `INPUT_UINPUT` not set, `HID_GENERIC=y`, `USB_HID=y`, `LEDS_CLASS_MULTICOLOR=m` (module not installed on the root file system). Built on the board with the kernel-module recipe in the FAQ: `hid-playstation.c`, `hid-ids.h` and `led-class-multicolor.c` from the TI kernel branch `ti-linux-6.18.y`; `insmod led-class-multicolor.ko hid-playstation.ko` succeeded. Not tested with a controller. A wired DualShock 4 binds `hid-generic` here (no `hid-sony`), so buttons come as plain HID events. The only Type-A port holds the stick the system runs from; a controller needs the SD boot first.

Rows: `storage_write_mbps` `sd-image-write-over-ssh`; `linux_boot_time` `sd-boot-to-login` (waiting).

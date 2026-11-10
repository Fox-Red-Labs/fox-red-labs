# TI LP-MSPM0L2228 LaunchPad on a Mac: errors and fixes

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes. Measured on the board, 2026-11-09.

## Connecting and flashing

### DSLite says a firmware update is required for the XDS110 probe. What now?

The board shipped on probe firmware 3.0.0.28, and DSLite stops with `A firmware update is required for the XDS110 probe`. Run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` to see the firmware, then write 3.0.0.43 with `xdsdfu` (keep the serial number; CCStudio's folder holds the image). After that DSLite connected and read the chip.

### Can the MSPM0L2228 be flashed with OpenOCD from a Mac?

Yes. xPack OpenOCD 0.12.0-7 with `interface/xds110.cfg` and `target/ti_mspm0.cfg` wrote and read back the blink image in 20.6 s; DSLite writes the same image in 4.2 s. `mdw 0x41c40004 2` returned `1bb9f02f 803c2c38`. OpenOCD's part table does not list the MSPM0L2228, yet it flashes it. The one-command script (`./start.sh mspm0l2228 blink`) uses this path. `data/results/mspm0l2228.csv`, tests `blink_flash` and `time_to_blink`.

### How do I read pins or RAM from a running image?

`DSLite memory -c <ccxml> -r <address>,<words> -o out.bin` reads without resetting the chip, and `mspm0l2228/bench/dsread.sh <address> <words>` prints the words. One read takes about 2 s because DSLite starts each time, so a half-period of a second or less cannot be timed this way; the output registers (GPIOA 0x400A1280, GPIOB 0x400A3280) only showed that they change.

## Building

### The build script says `sysconfig_1.26.3/sysconfig_cli.sh: No such file or directory`.

The script named SysConfig 1.26.3, and this Mac has 1.27.0 and 1.28.0. `python3 tools/frl/toolenv.py mspm0` prints the SysConfig in use; `mspm0l2228/bench/coremark/build.sh` reads it from there. CoreMark and the timing test were built with SysConfig 1.28.0.

### Two AES examples and two bsl examples fail with `symbol "interruptVectors" redefined`.

`aesadv_cmac_256_enc_dec`, `aesadv_gcm_128_enc_dec`, `bsl_software_invoke_app_demo_i2c` and `bsl_software_invoke_app_demo_uart` stop at the link step with `error: symbol "interruptVectors" redefined: first defined in ...` under TI Arm Clang 5.1.1.LTS. Not fixed; the other 172 of the 178 examples with a makefile build.

### The two kernel examples fail with `unknown argument: '-i/freertos_builds_LP_MSPM0L2228_release_ticlang/Debug'`.

`kernel/blink_led` and `kernel/posix_demo` (FreeRTOS) stop with `tiarmclang: error: unknown argument: '-i/freertos_builds_LP_MSPM0L2228_release_ticlang/Debug'`. The sweep builder does not handle their makefile; not fixed.

### On macOS, `sed -i` prints `extra characters at the end of g command`.

macOS `sed` needs an empty backup suffix: `sed -i '' 's/#define DELAY (16000000)/#define DELAY (4000000)/' gpio_toggle_output.c`. The first rebuild after the failed edit rebuilt the old delay.

### I changed a string in the example, rebuilt, and the board still prints the old text.

Seen with the UART console example: the first `make` ran, the edit followed, and a second `make` and `make clean` left the old image in place, so the board kept printing "==== MSPM0 Console Test ====". Edit the copied source before the first build, in a fresh copy, then run `make` once (`mspm0l2228/bench/builds/build.sh msg` does it in that order). Check the change by reading the console after the flash, not by the file's contents.

### A Python serial script says `No module named 'serial'`, and `stty -f` then `cat` prints junk.

The Mac's Python has no pyserial, and a baud set with `stty -f` reverts when stty closes the port. Open the port yourself with `termios` and the `IOSSIOSPEED` ioctl (0x80045402) and keep it open for the whole run; `mspm0l2228/bench/builds/msg_run.py` opens the console before the flash so the first line is not missed.

### On macOS, `paste -sd' '` with no file prints `usage: paste`.

BSD `paste` needs `-` to read standard input. Write `paste -sd' ' -`.

## The LEDs

### The blue LED does not blink on the LP-MSPM0L2228.

TI's `gpio_toggle_output` drives PA23 for blue, and the red LED PB10 and green PB9 blink as expected. The silkscreen on this Rev. A board labels the RGB jumpers PB9 (green), PB10 (red) and PA16 (blue), so blue is PA16 here. TI's user guide and the example describe PA23, which is a Rev E board; E2E thread 1497347 reports the same difference. Read the revision off the silkscreen and, on a Rev. A board, change the blue pin in the example's `.syscfg` to PA16. That change was not tried here.

### After restoring the shipped demo the red LED blinks, not the green one.

The pin read after the restore showed PA0 toggling on 4 of 4 reads; PA0 is the red LED LED3. TI's example README text names the green RGB pin. The README text and the shipped image may not match; the LEDs were not filmed, so only the pin read is recorded. `data/results/mspm0l2228.csv`, test `backup_restore`.

## The glass LCD

### What does TI's `lcd_write_memory` show?

Per its README, the six positions show digits 0 to 9, then letters A to Z, cycling through positions 1 to 6. Nobody looked at the glass. LCD control register 0 read `0x0000181D` and an LCD memory word changed between reads (`0x000A3A00`, then `0x000A7A00`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-09. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Not seen yet. Expected from the family: TI's low-power examples (sysctl_power_policy_sleep_to_*, *_standby, *_stop, sysctl_shutdown) cut the debug link (`mspm0g3507/AGENTS.md`, `docs/arrivals/mspm0l2228.md`)

Restore: frl restore <backup folder> --probe ML220001, or DSLite flash -c mspm0l2228/MSPM0L2228_ML220001.ccxml -e -f -v mspm0l2228/data/shipped-main-flash.bin,0x0; read back and compare. Regions: main flash (256 kB); NONMAIN read only, never written. (`data/results/mspm0l2228.csv`)

<!-- recovery:end -->

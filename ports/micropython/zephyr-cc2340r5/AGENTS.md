# What should an AI coding assistant know before building MicroPython on Zephyr for a TI LP-EM-CC2340R5?

That the port is two files, one config and one overlay, around MicroPython's own Zephyr port, and that the config exists because MicroPython's stock settings need 46.8 kB more RAM than the chip has. This file is the short list, each item from runs on one board.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [README](README.md) in the Fox Red Labs repo. In short: **west** is Zephyr's build front end; a **devicetree overlay** is a text file that adds hardware description to a board; **DSLite** is TI's command-line flasher (UniFlash 9.6.0); **CCFG** is the chip's boot configuration area. 1 kB = 1,024 bytes. Checked 2026-10-24 and 2026-10-25 on an Intel Mac, macOS 15.7, with MicroPython v1.29.0 (commit `0fd6c573e`), Zephyr 4.4.2 and Zephyr SDK 0.17.4 GCC 12.2.0, on one LP-EM-CC2340R5.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `ports/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `ports/micropython/zephyr-cc2340r5/README.md`.

## The port

| Part | Fact |
|---|---|
| What it is | Upstream MicroPython `ports/zephyr` built for the Zephyr board `lp_em_cc2340r5`, unmodified, with `-DCONF_FILE=prj_cc2340r5.conf` and `-DEXTRA_DTC_OVERLAY_FILE=lp_em_cc2340r5.overlay` (`ports/micropython/zephyr-cc2340r5/README.md`) |
| Size | 174.8 kB of flash (34.2 %) and 29.9 kB of RAM (83.0 %) with a 20 kB heap; the stock `prj.conf` overflows RAM by 46.8 kB (`ports/micropython/zephyr-cc2340r5/README.md`) |
| Works | Prompt on the probe's serial port at 115200, `Pin`, `ADC`, `Pin.irq()` (edges driven from software), `PWM` with `duty_ns`, `SPI` and `I2C` calls (`ports/micropython/zephyr-cc2340r5/README.md`) |
| Not run | SPI data (needs a loopback wire), the HDC2080 read, `I2C.scan()`, the LED blink (not watched) (`ports/micropython/zephyr-cc2340r5/README.md`) |
| Optional patch | `pwm-duty-u16-overflow.patch`, one cast in MicroPython's `machine_pwm.c` (`ports/micropython/zephyr-cc2340r5/README.md`) |
| No file system | Scripts are typed or pasted at the prompt; `CONFIG_MICROPY_VFS_*` is off (`ports/micropython/zephyr-cc2340r5/README.md`) |

## Start here

| Step | This port |
|---|---|
| Tools | MicroPython v1.29.0 at `<MP>`, Zephyr 4.4.2 west workspace at `<zephyr workspace>`, Zephyr SDK 0.17.4 GCC 12.2.0 at `<toolchain>`, `ninja`, `cmake` (`ports/micropython/zephyr-cc2340r5/README.md`) |
| Build | `west build -p always -b lp_em_cc2340r5 <MP>/ports/zephyr -d <build> -- -DCONF_FILE=<proj>/prj_cc2340r5.conf -DEXTRA_DTC_OVERLAY_FILE=<proj>/lp_em_cc2340r5.overlay`, with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile` and `CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-`; about 3 minutes |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder; the board has no USB port, so clip the probe on |
| Back up | `DSLite memory -c <ccxml> -r 0x0,0x20000 -o <backup>/main_flash.bin` and `-r 0x4E020000,0x200 -o <backup>/ccfg.bin`; keep the backup outside the repo |
| Flash | `DSLite flash -c <ccxml> -e -f -u <build>/zephyr/zephyr_crc32.hex` (the `_crc32` file carries the boot checksums) |
| Check | `python3 repl.py /dev/cu.usbmodem<probe serial>1 out.log --reset -- 'import machine' 'print(machine.ADC(("adc0", 2)).read_u16())'`; the banner reads `MicroPython v1.29.0 ... zephyr-lp_em_cc2340r5` |
| Restore | `DSLite flash` of a hex merged from the two backup files, then read both areas again; they match byte for byte |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| ``ld.bfd: region `RAM' overflowed by 47888 bytes`` | MicroPython's `prj.conf` turns on networking and a 48 kB heap | Use `prj_cc2340r5.conf` | Image links (`ports/micropython/zephyr-cc2340r5/README.md`) |
| ``relocation truncated to fit: R_ARM_THM_JUMP11 against symbol `nlr_push_tail` `` | `-DCONF_FILE=prj_minimal.conf` selects `mpconfigport_minimal.h`, which lacks `MICROPY_NLR_THUMB_USE_LONG_JUMP` on a Cortex-M0+ | Keep `CONFIG_MICROPY_CONFIGFILE="mpconfigport.h"` | Image links |
| ``ImportError: no module named 'time'`` | The core-features level leaves out `time` | Use basic features or higher | `import time` works |
| ``AttributeError: 'Pin' object has no attribute 'toggle'`` | The Zephyr port's `Pin` has `on`, `off`, `value` only | `led.value(1 - led.value())` | LED state flips |
| `machine.ADC` reads 0 or 65,5xx after Ctrl-D | A soft reboot does not reset the hardware; a pad made an output stays one | `machine.reset()` | Reads change with the pin |
| `PWM(..., duty_u16=n)` gives a much smaller duty at low frequencies (1 kHz, 16,384 gave 2,562 of 48,000 counts) | `machine_pwm.c` multiplies in 32 bits and overflows | Use `duty_ns`, or apply `pwm-duty-u16-overflow.patch` | Compare register reads 12,000 |
| `PWM(("pwm5", 0), ...)` changes nothing on the red LED | The red LED pin is on LGPT1 channel 2; the tuple's second item is the timer channel | `PWM(("pwm5", 2), ...)` for red, `PWM(("pwm6", 0), ...)` for green | LGPT1 compare changes |
| A pin's `irq()` stops firing after `Pin.init()` | Zephyr 4.4.2's `gpio_cc23x0` rewrites the IOC register without the edge-detect bits | Call `irq()` again after `init()` | Handler runs 10 of 10 |
| `I2C.scan()` returns `[]` while `readfrom_mem` at a known address works | The scan's empty write fails on the CC23x0 I2C driver (not traced) | `readfrom_mem(addr, reg, 1)` inside `try` | Device answers |

All rows: `ports/micropython/zephyr-cc2340r5/README.md`, "What went wrong".

## Rules for the assistant

1. **Do not edit the MicroPython or Zephyr trees.** A change goes in the conf or the overlay; the one exception is the optional `pwm-duty-u16-overflow.patch` (`ports/micropython/zephyr-cc2340r5/README.md`).
2. **Size the heap from the link summary.** Static RAM outside the heap is 9.9 to 10.0 kB in every build, so the largest heap that links is about 26 kB (`ports/micropython/zephyr-cc2340r5/README.md`).
3. **Name pins by Zephyr label and number**: `("gpio0", 15)`, `("adc0", 2)` where 2 is the ADC channel from the overlay, not the DIO number (`ports/micropython/zephyr-cc2340r5/README.md`).
4. **Back up main flash and CCFG before the first write** and compare after the restore (`ports/micropython/zephyr-cc2340r5/README.md`).

# AM62L family porting FAQ: errors seen moving code to the TMDS62LEVM

Errors hit while moving code onto the AM62L EVM, with the fix and the file behind each. Board-only errors are in `am62l-evm/FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### There is no compiler on the board, and `docker build` sits at 0 percent CPU

Build in a Debian trixie arm64 container started with `--network host`, and install packages into the running container instead of using `docker build` or `docker commit`. `am62l-evm/AGENTS.md`, `am62l-evm/steam/setup.sh`.

### `opkg update` or `opkg install` cannot download anything, and there is no `apt`

Use `docker run` or `pip3 install --break-system-packages`. The package feed host does not resolve outside TI's network. `am62l-evm/linux-apps/README.md`.

### `pip install lgpio` fails, gpiozero says `PinUnknownPi`, or Blinka cannot identify the board

lgpio needs `swig==4.3.1` and a source-built C library; gpiozero needs a board description (`am62l-evm/linux-apps/gpiozero_demo.py`); Blinka needs `am62l-evm/linux-apps/blinka_am62l.py`. `am62l-evm/FAQ.md`.

### The Zephyr build fails in Kconfig with a path error

Move the workspace to a path without a space; a symlink does not help. `am62l-evm/zephyr/README.md`.

### The 144 kB Zephyr SRAM image panics at start-up in the simulator, and the 80 kB image will not link

Use 8 translation tables (`bare-el3.conf`, 32 kB); 6 are too few. The 80 kB MAIN SRAM map does not link even with every cut; 104 kB is the floor. `am62l-evm/zephyr/README.md`.

### How do I build a kernel module on the board?

Copy the SDK kernel source over, use the board's `/proc/config.gz` with a matching `CONFIG_LOCALVERSION`, run `modules_prepare`, then build; `insmod` accepts it because MODVERSIONS is off. `am62l-evm/FAQ.md`, `am62l-evm/rpmsg/`.

## Running

### The serial port is silent, or a 115200 line looks silent

Hold the port open in the program that reads it. On macOS `stty -f` resets when `stty` exits, so `cat` reads at 9600. `am62l-evm/tools/capture.py`.

### The console stays silent with the switches as shipped

The boot ROM is waiting for an SD card. Boot over UART (SW3.4 ON, 3.3 ON, 3.2 ON, 3.1 OFF, full power cycle, `uart_boot.py`). `am62l-evm/README.md`.

### DSLite says `An invalid processor ID has been found`

Add `cortexA53`, `cortexM`, `etbcs`, `csstm` and `ctset2` to the ccxml; the template does. The ccxml listed only the DAP driver. `am62l-evm/tools/AM62L_TMDS62LEVM.ccxml.template`.

### DSLite cannot connect to core 1: error -1062

Start core 1 with PSCI `CPU_ON` from U-Boot (`am62l-evm/tools/psci_stub.py`). The core is powered down. `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### U-Boot prints `Transfer data timeout`, then `Card did not respond to voltage select! : -110`

Reseat or replace the card; the eMMC still answers, so UART boot plus a USB stick works. A bad or unseated card. `am62l-evm/AGENTS.md`.

### SD boot shows ROM `C`, `BL1` notices, `ENTERING WFI - end of bl1`, then silence

Push the card fully in, put SW3 all OFF with SW4 and SW2 as shipped, unplug J17 for 30 s, power on. U-Boot then printed `MMC: no card present` while the ROM still read the card. `am62l-evm/FAQ.md`, `docs/experiments/2026-10-24-am62l-sd-boot-stall.md`.

### `booti` prints `Starting kernel` and nothing else

Run `setenv bootargs` first (`printenv bootargs` shows it). Only a reset recovers the board. `am62l-evm/README.md`.

### A UART boot sent late gets `receiver cancelled` or `block not acknowledged`

Power cycle and send within the ROM's window (5 minutes after power); a `reboot` from default Linux returns to the ROM ping. `am62l-evm/README.md`.

### Zephyr in on-chip SRAM beside Linux fails or breaks Linux

Link Zephyr in DDR at 0xE0000000 and boot Linux with `mem=1536M`. Linux owns 0x70800000 to 0x7080FFFF and TF-A code sits at 0x707F0000. `docs/experiments/2026-10-22-am62l-linux-zephyr-amp.md`.

### Zephyr on core 1 stops Linux's interrupts

Build Zephyr with `am62l-evm/zephyr/patches/gic-skip-distributor.patch` so it touches only its own core's GIC registers. Linux and Zephyr both want the GIC distributor. `am62l-evm/zephyr/README.md`.

### A program dies with `Illegal instruction`

Read the address from `dmesg` (`echo 1 > /proc/sys/debug/exception-trace`), check for an LSE instruction, and preload a trap library from `am62l-evm/steam/lse_trap/`. The A53 lacks ARMv8.1 LSE atomics; the kernel runs armhf programs (`CONFIG_COMPAT=y`). `am62l-evm/AGENTS.md`.

### The LED line reads 0 whatever I write

Keep the state in your program, check the output register with `gpio_regs.py`, and free the line with `gpio_claim.sh free`. LD8's pad reads back 0. `am62l-evm/linux-apps/README.md`.

### A program's serial lines end in `\r\r\n` and a line checker sees twice as many lines

Clear `OPOST` in the program and run `stty -F /dev/ttyS0 opost` afterwards. `portability/three-ways/linux/README.md`.

### `docker run -p 1883:1883` says the address is already in use

`systemctl stop mosquitto` first. TI's image runs Mosquitto on 127.0.0.1:1883. `am62l-evm/AGENTS.md`.

## Mistakes to avoid

- Do not run SRAM images beside Linux.
- Do not trust a boot-mode table read top to bottom; read each switch number on the silkscreen.
- Do not hot-plug the USB stick into a running board.

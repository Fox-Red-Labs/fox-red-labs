<!-- experiment
id: E185
board: Raspberry Pi Pico 2 H
date: 2026-10-11
status: line found
reached: compile
line: Zephyr 4.4.2's blinky does not compile for rpi_pico2/rp2350a/hazard3 because that board file has no led0 alias; a 15-line overlay that adds it makes it compile (39,424-byte UF2, not flashed).
stop: the missing led0 alias in the board file
-->
# Why does Zephyr's blinky not compile for the Pico 2's RISC-V cores, and what fixes it?

**Short answer.** The board file for `rpi_pico2/rp2350a/hazard3` leaves out the LED description that the Cortex-M33 board file includes, so the `led0` alias that blinky needs does not exist. An overlay that defines the LED on GP25 makes blinky compile. It was built and not flashed.

Terms: [vocab](../vocab.md). Board problems and fixes: [FAQ](../../pico2/FAQ.md). 1 kB = 1,024 bytes. West is Zephyr's build command. A devicetree overlay is a small file that adds to the board's hardware description. An alias such as `led0` is a short name blinky uses to find the LED. GP25 is the Pico's LED pin. pico-sdk-tools is Raspberry Pi's release page for the RISC-V compiler.

**Question.** Why does `west build -b rpi_pico2/rp2350a/hazard3 samples/basic/blinky` fail, and what is the smallest fix?

**Vendor position.** Zephyr 4.4.2 lists the board `rpi_pico2/rp2350a/hazard3` and its `.yaml` names gpio among the supported features (read 2026-10-11, `boards/raspberrypi/rpi_pico2/`).

**What was tried.** Zephyr 4.4.2 with the cross-compile toolchain variant and the RISC-V GCC 16.1.0 from Raspberry Pi's pico-sdk-tools. The build stops at the compile of `samples/basic/blinky/src/main.c` with `error: '__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here (not in a function)` and `error: 'DT_N_ALIAS_led0_P_gpios_IDX_0_VAL_pin' undeclared here`. The cause is in the board files: `rpi_pico2_rp2350a_m33.dts` includes `../common/rpi_pico-led.dtsi` (the `led0` alias on GPIO 25) and `rpi_pico2_rp2350a_hazard3.dts` does not, and `rpi_pico2.dtsi` defines no LED. The build used `-DDTC_OVERLAY_FILE=pico2/zephyr/hazard3-led.overlay` (in the repo), which adds the same `leds` node on GPIO 25 and the `led0` alias: a `/ { leds { compatible = "gpio-leds"; led0: led_0 { gpios = <&gpio0 25 GPIO_ACTIVE_HIGH>; }; }; aliases { led0 = &led0; }; };` block. The toolchain variant is `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, because the Zephyr SDK is not installed on this Mac.

**How far it got.** Compile and link: the build with the overlay finishes and writes a 39,424-byte `zephyr.uf2`. Not flashed: a Zephyr image is expected to have no USB reset interface (not checked), in which case loading it needs a button press on the board.

**The line.** None after the overlay. Before it, the line is the missing alias in one board file; the Cortex-M33 build finished (recorded in the `zephyr_support` row of the results store).

**Ask.** For the Zephyr maintainers (not asked): is the missing include on purpose?

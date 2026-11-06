# Zephyr and LVGL on the LP-MSPM0G3519: flash and RAM cost

**Short answer.** 253.5 kB of flash and 49.5 kB of RAM for a two-widget screen with a dummy display, built 2026-10-10 with Zephyr 4.4.2 and its LVGL 9.5.0. It cannot draw on the real panel: Zephyr has no MSPM0 SPI or I2C driver. Details: [the experiment page](../../docs/experiments/2026-10-10-mspm0g3519-zephyr-lvgl.md). Terms: [vocab](../../docs/vocab.md).

Build (the toolchain the board's other Zephyr builds use):
`cd ~/zephyrproject/zephyr && . ../.venv/bin/activate && ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi- west build -b lp_mspm0g3519 <this folder> -d <build dir> -- -DDTC_OVERLAY_FILE=<this folder>/app.overlay` after `west update lvgl`. Not flashed.

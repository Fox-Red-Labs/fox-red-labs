# Run the ESP32-BLE-Gamepad check on an ESP32-C6

Terms: [vocab](../../../../docs/vocab.md); parent page: [ESP32-BLE-Gamepad on TI boards](../../README.md).

The ESP32-C6 is the reference: it runs the unmodified library and produces the output every other board is compared with. `harness/frl_gp_check.ino` runs the unmodified library (it opens the library's private members to read the descriptor and call the packer) and prints the FRL-GP1 block; `frl_gp_core_c6.ino` runs the G3519 port's `frl_gamepad.c` on the same chip with the same block.

    arduino-cli lib install NimBLE-Arduino@2.5.1
    arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc --library <clone of ESP32-BLE-Gamepad at 65d5178> harness/frl_gp_check

For the core-only sketch, copy `boards/mspm0g3519/frl_gamepad.c` and `.h` beside `frl_gp_core_c6.ino`. The USB serial port re-enumerates on every reset, so `check/gp_serial.py` reopens it.

<!-- experiment
id: E40
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: works
reached: verified
line: The vendor-free BTHome header drops onto Espressif's NimBLE_Beacon example with 40 lines of glue against TI's 58. That is 35 in one new file and 5 changed elsewhere. The Mac decodes the packets as version 2, with temperature and counting packet ids.
stop: none
-->
# Does a vendor-free BTHome header work on the ESP32-C6 with less glue than on the TI CC2340R5?

**Short answer.** Yes. `ports/bthome/common/bthome.h` built into Espressif's `NimBLE_Beacon` example with no change to the header. The port is one new file of 35 code lines plus 5 changed lines in the example, 40 in all, against 58 for the TI port. The Mac's Bluetooth scanner decoded the packets as BTHome version 2, unencrypted, with a packet id that counts up and the chip temperature. Home Assistant was not used: the board is the house Bluetooth proxy, so the Mac scanner is the proof.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. BTHome is the open advertising format that Home Assistant reads with no pairing (bthome.io).

**Question.** Espressif's NimBLE beacon already builds raw advertising data. Does the same header that went onto the TI board drop on with fewer lines of glue?

**Vendor position.** Espressif ships the example as `examples/bluetooth/ble_get_started/nimble/NimBLE_Beacon` in ESP-IDF v6.1: a non-connectable advertiser with a fixed name, appearance and URI. It has no sensor and no BTHome. Read on 2026-10-11.

**What was tried.** ESP-IDF v6.1 with newlib and GCC 15.2.0, one ESP32-C6-DevKitC-1-N8, the example copied to `ports/bthome/esp32c6/NimBLE_Beacon/`.

- New `main/src/app_bthome.c` has 47 lines, 35 of them code. It installs the chip temperature sensor and builds the payload through `bthome.h`: packet id, then temperature in hundredths of a degree. It hands the payload to `ble_gap_adv_set_fields` as 16-bit service data for UUID 0xFCD2 and repeats every 10 s from a timer. Advertising is non-connectable at a 1 s interval, name "FRL C6".
- `main/src/gap.c`: one declaration and one call to `bthome_start` in place of the stock `start_advertising()`. It adds 3 lines and removes 1.
- `main/CMakeLists.txt`: two components added to the requirements and one include directory for `common/`.

Lines are counted the way the TI port counts them: non-blank lines that are not comment-only (`grep -vcE '^\s*$|^\s*(//|/\*|\*)'`). Run it: build with `idf.py` (newlib) and flash with `idf.py flash`, then run `ports/bthome/host/bthome_scan.py 75 "FRL C6"` on the Mac.

**How far it got.** Verified, first build. Build 604 kB of application image and 89.8 kB of the 441.5 kB of static RAM. Flash took 4.2 s. The serial line printed `BTHome: 36.4 C, packet 0, set_fields 0` at boot and a new packet every 10 s. The Mac scanner, using bleak and CoreBluetooth, printed, for example:

```text
'FRL C6' rssi=-33 raw=40000302a80c {'version': 2, 'encrypted': False, 'trigger': False, 'packet_id': 3, 'temperature_c': 32.4}
'FRL C6' rssi=-34 raw=40000502440c {'version': 2, 'encrypted': False, 'trigger': False, 'packet_id': 5, 'temperature_c': 31.4}
```

The scanner showed ids 3, 4 and 5 and did not print ids 0 to 2, which the serial line says went out in the first 30 s. The scanner was started two seconds before the flash and printed nothing until id 3; the cause, in the Mac's scan start-up, was not isolated.

**The line.** No limit reached. Changing the advertising fields while the set is advertising is allowed in NimBLE, and `set_fields` returned 0 each time. The port needs no stop and restart. The TI port does, in `GapAdv_prepareLoadByHandle`. The die temperature reads 31 to 36 C against a room near 21 C: it is the chip's own sensor right after boot and was not checked against a thermometer, the same caveat as the TI board. Not in Home Assistant, and not encrypted.

**Ask.** None.

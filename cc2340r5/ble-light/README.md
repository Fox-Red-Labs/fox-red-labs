# Bluetooth light on the LP-EM-CC2340R5 (CC2340R52)

Terms: [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md). Status: Should work. The firmware builds for this board and has not run on it.

A phone page controls the kit's LEDs over Bluetooth Low Energy. It is the same firmware as the CC2755P10 one, built from the same source. The service UUID, characteristics, LED pins and what is untested are in [cc2755p10/ble-light/README.md](../../cc2755p10/ble-light/README.md).

Build it from the top of the repository:

```bash
cc2340r5/ble-light/build.sh
```

The output lands in `prebuilt/cc2340r5/ble-light/`. The LEDs are red on DIO14 and green on DIO15. Built 2026-10-15 with SimpleLink Low Power F3 SDK 9.21.00.36.

Next: flash it, then connect from a phone browser that supports Web Bluetooth and write a color.

# The tool tag on Zephyr: one source for TI, Nordic and ST Bluetooth chips

The same tag as `../tag/` (TI's BLE5-Stack), written against Zephyr's Bluetooth API: same UART lines in, same legacy and extended advertising out, same on-tag pull classifier. The hub, the phone app and the home base cannot tell the two apart. The format and the model are the shared C files (`../proto/tt_proto.c`, `../ai/out/tt_model.c`), compiled unchanged; `src/main.c` is the only Zephyr code, about 200 lines.

Terms: [docs/vocab.md](../../docs/vocab.md). Formats: [PROTOCOL.md](../PROTOCOL.md).

| Board | Chip vendor | Zephyr | Result | Flash | RAM |
|---|---|---|---|---|---|
| LP-EM-CC2340R5 | TI | TI's fork v3.7.0-ti | **Ran** with the CC35X1 hub: 117 of 117 events (median 2.4 s), 105 of 110 pull summaries, totals identical, on-tag model 96.2% right | 111.2 kB | 31.5 of 36 kB |
| nRF52840 DK | Nordic | 4.4.2 | Built, not run (no board here) | 73.7 kB | 22.2 kB |
| NUCLEO-WB55RG | ST | 4.4.2 | Built, not run (no board here) | 41.2 kB | 15.4 kB |

## Build

```
tool-telemetry/tag-zephyr/build.sh                         # nRF52840 DK and NUCLEO-WB55RG on Zephyr 4.4.2
```

For the LP-EM-CC2340R5, upstream Zephyr 4.4.2 has no Bluetooth controller for the chip; use TI's fork, set up as in [cc2340r5/zephyr-ti/README.md](../../cc2340r5/zephyr-ti/README.md), then:

```
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd ~/zephyr-ti/ws/zephyr && west build -p always -b lp_em_cc2340r5 <repo>/tool-telemetry/tag-zephyr -d <scratch>/runs/tool-telemetry/tag-zephyr/lp_em_cc2340r5-tifork
```

Flash `zephyr.hex` with UniFlash (`dslite.sh --mode flash -e -f -u`). The console UART is the tool link: tool lines start with `$`, everything else is ignored.

## Differences from the BLE5-Stack tag

- Events and pulls are kept in RAM, so a restart loses them (the BLE5-Stack tag keeps events in flash). The hub notices the restart and keeps logging.
- No two-button demo mode.
- The legacy set is not scannable, so it carries no name; the extended set carries it.
- The workspace needed Nordic's HAL and Zephyr's mbedTLS and TF-PSA-Crypto modules (`west update hal_nordic mbedtls tf-psa-crypto`); the ST and TI builds need host crypto for Bluetooth addresses.

## Limits

Only the TI board ran here. The Nordic and ST builds link; whether their controllers carry a 172-byte extended frame at a 1 s interval is untested.

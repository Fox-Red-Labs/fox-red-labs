# What works on the TI LP-EM-CC2340R5, and where does each radio stack stop?

Terms: RCP, DMM, OTA, ZHA, MCUboot and CCFG are defined in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

Bluetooth LE and a Zigbee router into Home Assistant work on this board from TI's SimpleLink Low Power F3 SDK 9.21.00.36 examples, with a few settings changed. Most of the rest fails to link for lack of RAM (the chip has 36 kB) with TI's configurations, and the remainder stops at a missing TI port or a fault not found.

In six lines:

1. **Works:** Bluetooth LE (advertise, connect, echo) and a Zigbee router into Home Assistant. A Zigbee end device (TI's on/off switch example) rejoined the network 15 s after flashing, measured once; its device type and sleep behavior were not confirmed. TI's OTA client example, also an end device, joined and was listed as an end device.
2. **RAM decides:** 36 kB. Left for your code: 5.1 kB with Bluetooth, 6.2 kB with a Zigbee end device, 10.7 kB with the Zigbee OTA client, 8.8 kB with the Thread RCP (flashed and run, 2026-10-06), 799 B with a Zigbee router with logging on.
3. **Compiles, fails to link (RAM):** a full Thread device (7.5 kB to 23.8 kB over, depending on build options) and Bluetooth plus 802.15.4 at once (DMM, 2.1 kB short of the chip's total free RAM before any heap). **Unknown (suspected: Chip limit):** only TI's configurations were tried, built with this repo's settings; a trimmed build might fit; not run. Both link on the 64 kB CC2340R53.
4. **Builds, does not run:** Bluetooth on TI's Zephyr fork. hello_world ran after a boot-configuration change; Bluetooth samples printed nothing. **Unknown.**
5. **Starts, never finishes:** a Zigbee over-the-air update from Home Assistant. Best 1.34% in four transfers. **Unknown (suspected: Radio environment).**
6. **Not offered for this chip:** Matter, Thread under Zephyr, Bluetooth under upstream Zephyr. ESPHome's Zigbee end device links (32.0 kB of the 36 kB of RAM; not run); its router build is 1.3 kB over.

Tested 2026-10-05 into 2026-10-06 on one board with an XDS110 probe, from an Intel MacBook (macOS 15.7.9) with TI Arm Clang 5.1.1, SysConfig 1.26.3 and UniFlash 9.6.0; Home Assistant ran ZHA with a ZBT-2 radio on a separate host. The board carries the CC2340R52 chip (36 kB of RAM); the 64 kB CC2340R53 is on a separate board, LP-EM-CC2340R53. 1 kB = 1,024 bytes.

## Verdict for each combination

| Combination | Verdict | Cause | Detail |
|---|---|---|---|
| Bluetooth LE, TI SDK | **Works** | | Advertised, connected, echo checked |
| Bluetooth LE, upstream Zephyr 4.4.2 | **No** | **Third-party behavior** | Board file lists GPIO and UART only |
| Bluetooth LE, TI's Zephyr fork | **Builds; Bluetooth samples print nothing** | **Unknown** | hello_world ran after a boot-configuration change (CCFG byte 0x4E02001C); Bluetooth samples printed nothing ([zephyr-ti](../zephyr-ti/README.md)) |
| Zigbee router, TI SDK | **Works** | | Joined Home Assistant; 20 of 20 commands landed; 95 ms median at first pairing; about 5 s after re-pairing, cause unknown ([zigbee](../zigbee/README.md)) |
| Zigbee end device, TI SDK | **Joined; type confirmed only for the OTA client example** | **Unknown** (type, sleep) | `onoff_switch` rejoined 15 s after flashing, measured once; Home Assistant kept its old router record |
| Bluetooth plus 802.15.4 at once (DMM) | **Compiles, fails to link (RAM) on the R52; links on the R53** | **Unknown (suspected: Chip limit)** | 2.1 kB short of the chip's total free RAM before any heap; a trimmed build might fit; not run |
| Thread | **RCP only per TI; RCP flashed and run (joined a CC1354P10 leader, 2026-10-06); node builds fail to link (RAM)** | **Unknown (suspected: Chip limit)** | Node builds 7.5 kB to 23.8 kB over, depending on build options; a trimmed build might fit; not run ([thread](../thread/README.md)) |
| Thread, Zephyr | **No** | **Tool or docs** | No 802.15.4 driver for CC23xx in TI's fork |
| Matter | **No** | **Tool or docs** | TI's Matter fork has CC27xx examples only; not built |
| Proprietary 2.4 GHz packets | **Builds** | | `rfPacketTx`; not run, needs a second radio |
| Bluetooth over-the-air update | **Works from a Mac** | | One run on 2026-10-07 with this repo's `tools/oad/oad_client.py` and TI's dual-image example; TI's own senders are a phone app or a Windows program ([oad](../oad/README.md)) |
| Zigbee over-the-air update | **Starts, never finishes** | **Unknown (suspected: Radio environment)** | Best 1.34% of 200.1 kB |
| SysConfig without the GUI | **Works** | | About seven settings tried as `.syscfg` text edits; Bluetooth changes seen on air |
| ESPHome | **End device links, not run; router build does not fit** | **Chip limit** | The router build needed 37.4 kB of the 36 kB of RAM ([experiment](../../docs/experiments/2026-10-13-cc2340r5-esphome-fit.md)) |
| Clock speed | **48 MHz** | | TI documents only 48 MHz; read back at 48.0 MHz once |

"No" verdicts say which kind of no: **Tool or docs** when TI offers nothing for this chip, **Chip limit** when the chip cannot (written as suspected when only TI's configurations were tried), **Third-party behavior** when the gap is in non-TI software.

Why the OTA cause is suspected, not proven: the coordinator's readings of this board that evening, none taken during a transfer, were LQI 36 to 48 and RSSI -88 to -91 dBm, a weak link.

## More on this board

- RAM per stack: `memory-breakdown.json` in this folder, and the rows `radio_ram_left` and `radio_flash_used` in `data/results/cc2340r5.csv`.
- Bluetooth, Zigbee and CPU numbers: the same CSV.
- Write-ups: [DMM](../../docs/experiments/2026-10-05-cc2340r5-dmm.md), [Thread](../../docs/experiments/2026-10-05-cc2340r5-thread-full-device.md), [Zigbee OTA](../../docs/experiments/2026-10-05-cc2340r5-zigbee-ota-home-assistant.md), [TI's Zephyr](../../docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md), [SysConfig](../../docs/experiments/2026-10-05-cc2340r5-sysconfig-without-gui.md).

## Not done

Sleep current, energy, packet error rate and range; a full Thread border router with this chip as its radio ([thread](../thread/README.md); the CC2340R5 RCP already drove a CC1354P10 host in [the border-router host](../../border-router/host-cc1354/README.md)); the debugger on TI's Zephyr Bluetooth samples, which print nothing ([zephyr-ti](../zephyr-ti/README.md)).

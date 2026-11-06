# Can the LP-CC1354P10-1 (CC1354P10) hear the CC1407P weather station with TI's own receive example?

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

**Short answer.** Yes. `rfPacketRx`, TI's proprietary-radio receive example for the LP-CC1354P10-1, with two files changed hears the LP-MSPM0G5187 and its BP-CC1407P-9-FE radio: 21,658 of 21,658 packets over about 6 h 15 min, ending 2026-10-11 06:00, no checksum errors, at -14 dBm on the same desk. Its default 50 kbps 2-GFSK settings match TI's CC1407P settings in every field compared, and the checksum agreed in practice, so only the channel changes, from 868 to 915 MHz. A bridge on the Mac posts the readings to Home Assistant. Run 2026-10-10 and 11; the sender is in [mspm0g5187/radio](../../mspm0g5187/radio/README.md).

Checked on an Intel Mac, macOS 15.7.9, with TI Arm Clang 5.1.1.LTS (from CCStudio 21.0.1), SysConfig 1.28.0 (the sender's MSPM0 project used 1.26.3), UniFlash 9.6.0, F2 SDK 8.33.00.16 from GitHub and TI's prop_rf examples tag `lpf2-lpf3-8.33.00.16-9.21.00.36_LTS-prop_rf`.

The link in one line: sender LP-MSPM0G5187 plus BP-CC1407P-9-FE, receiver this board, 915 MHz, 50 kbps, text packets of 37 to 39 bytes every 1.04 s.

## Packet format: the two radios agree

Compared from the two settings files, TI's generated `ti_radio_config.c` for this board and TI's `rcl_settings_2gfsk50kbps.h` for the CC1407P: 50 kbps symbol rate, 25 kHz deviation, 4-byte preamble, 32-bit sync word 0x930B51DE, a 1-byte length field, no whitening. The checksum was not compared on paper; in practice it agreed, with 0 checksum errors in 21,658 packets.

## What changed from TI's example

- `rfrx.syscfg`: `phyInst.carrierFrequency = 915`, and a serial port (UART2) on the XDS110 debugger's backchannel.
- `rfrx.c`, in place of `rfPacketRx.c`: it keeps packets that fail the checksum, so a format mismatch shows instead of silence; it appends RSSI (received signal strength); and it prints every packet plus, about every 14 s, the radio core's own counters of good and bad packets: `count ok=350 crc_bad=0`. The counters are 16-bit, so they wrap after 65,535 packets, about 19 hours at this rate.
- `main_nortos.c`, `RFQueue.c`, `RFQueue.h`: TI's, unchanged.

Each packet prints as hex; the payload is plain text:

```text
pkt 2 len=38 rssi=-14 status=0x00 data=46524c206e3d32393220743d32352e3231432072683d33362e3025206c75783d3230382e3438
```

`46524c20...` reads `FRL n=292 t=25.21C rh=36.0% lux=208.48`. `status=0x00` means the checksum passed.

## Build

The F2 SDK 8.33.00.16 on GitHub (`TexasInstruments/simplelink-lowpower-f2-sdk`, tag `lpf2-8.33.00.16`) has no prebuilt driverlib, TI Drivers or no-RTOS libraries; TI's installer has them. The example lives in `TexasInstruments/simplelink-prop_rf-examples`. About 5 minutes and 110 MB:

```bash
git clone --filter=blob:none --no-checkout --depth 1 --branch lpf2-8.33.00.16 https://github.com/TexasInstruments/simplelink-lowpower-f2-sdk.git f2sdk
cd f2sdk && git sparse-checkout init --no-cone
printf '/*\n!/*/\n/.metadata/product.json\n/source/ti/devices/\n/source/ti/drivers/\n/source/ti/boards/\n/source/ti/display/\n/source/ti/log/\n/source/ti/common/\n/source/ti/posix/\n/source/ti/utils/\n/source/ti/dpl/\n/source/ti/prop_rf/\n/source/ti/*.js\n/source/ti/.meta/\n/source/third_party/CMSIS/\n/source/cmake/\n/kernel/nortos/\n' > .git/info/sparse-checkout
git checkout HEAD && cd ..
python3 cc1354p10/radio-rx/build_f2_libs.py f2sdk
```

The library builder compiles 43 of 43 driverlib files, 14 of 14 no-RTOS files and 83 of 90 driver files from the SDK's own CMake source lists; CAN, SD card and PDM microphone drivers fail and the receiver does not use them. Then clone the prop_rf examples tag, copy `examples/nortos/LP_EM_CC1354P10_1/prop_rf/rfPacketRx`, put `rfrx.c` and `rfrx.syscfg` in as `rfPacketRx.c` and `rfPacketRx.syscfg`, and in its `ticlang/` folder:

```bash
make SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=<path>/f2sdk SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS GCC_ARMCOMPILER= IAR_ARMCOMPILER=
```

Flash `rfPacketRx.hex` with UniFlash's DSLite and `cc1354p10/CC1354P10.ccxml` (your probe serial in it), then start it with `xds110reset -s <probe serial>` from CCStudio's `ccs_base/common/uscif/xds110/`. The image is 29.2 kB of flash.

## Into Home Assistant

`ha_bridge.py` reads the receiver's serial port and posts four sensors through Home Assistant's REST API every 10 seconds: `sensor.frl_radio_temperature`, `sensor.frl_radio_humidity`, `sensor.frl_radio_light` and `sensor.frl_radio_signal`, with units and device classes so they graph.

```bash
python3 cc1354p10/radio-rx/ha_bridge.py --port /dev/cu.usbmodem<probe serial>1
```

Give it your Home Assistant address and a long-lived access token in the environment variables `HA_URL` and `HA_TOKEN` (or `--url` and `--token`, but a token on the command line shows in the process list). Plain `http://` is accepted only for a local-network or private VPN address such as `http://homeassistant.local:8123`; anything else needs `https://`, and redirects are refused so the token never goes to another host. A token is made in Home Assistant under your profile, Security, Long-lived access tokens. `--follow <log file>` reads a log another program is writing instead, for when that program holds the port. The entities come from the REST API, not an integration, so after a Home Assistant restart they are missing until the next post, at most 10 s later. One run on 2026-10-11 made 253 posts from 06:03.

`loss_run.py <seconds> <folder>` logs the sender's and receiver's serial ports at once with timestamps and compares the receiver's counters with the sender's count, skipping the lines the serial bridge replays in the first 3 s. One hour on 2026-10-11: 3,508 of 3,508, 0 checksum errors.

## Other protocols from the same radio

The CC1407P has 23 protocol settings ([sender README](../../mspm0g5187/radio/README.md#every-protocol-the-radio-has)). This board's radio has SysConfig settings for four of its families, so those were checked over the air on 2026-10-11, 75 s each, with the sender looping 20 numbered 60-byte packets per protocol:

| CC1407P protocol | Receiver setting in SysConfig | Result |
|---|---|---|
| 2-GFSK 50 kbps: high performance, general purpose and low power | the default 50 kbps 2-GFSK | 10 of 10 of each at 20 bytes in the 23-profile run; high performance also 80 of 80 in the SPI sweep and the six-hour run above |
| Wi-SUN FSK mode 2b, 100 kbps | `2gfsk100kbps50dev915wsun2b` | 260 of 260 sent, 13 complete batches, 0 checksum errors |
| Wi-SUN FSK mode 4b, 200 kbps | `2gfsk200kbps100dev915wsun4b` | 260 of 260 sent, 13 complete batches, 0 checksum errors |
| SUN O-QPSK rate mode 3 | `qpsk50kbpsrm3`, which SysConfig labels "Release Candidate" | 260 sent; every reception had a bad checksum (220 by the last count line) |

The two Wi-SUN settings receive with TI's `CMD_PROP_RX_ADV`, which reads the 802.15.4g two-byte header. Build `rfrx.c` with `#define RX_ADV` and `#define MAX_LENGTH 255` at its top, and add to `rfrx.syscfg`:

```text
phyInst.phyType868 = "2gfsk100kbps50dev915wsun2b";
phyInst.carrierFrequency = 915;
phyInst.codeExportConfig.cmdList_prop = ["cmdPropRadioDivSetupPa", "cmdFs", "cmdPropTxAdv", "cmdPropRxAdv"];
```

The sent count comes from the sender's own batch lines. The receiver's count line lags its printout; at its last print it read 220 and 200.

Why O-QPSK fails is suspected, not proven. The receive command SysConfig generates for that setting expects TI's proprietary frame (sync word 0x930B51DE, a 1-byte length), and the CC1407P's driver names its O-QPSK header for 802.15.4 SUN O-QPSK; no sniffer checked what is on the air. No matching setting was found in this board's SysConfig list (F2 SDK 8.33 radio settings, read 2026-10-11) for the 38.4 and 9.6 kbps GFSK, OFDM, OFDM-LR and XLR protocols; the one 9.6 kbps setting is a narrowband 868 MHz one. OFDM, OFDM-LR and XLR need a second CC1407P to receive; the two GFSK rates might work with a custom setting, not run.

## As a test partner for the CC1407P

`partner.c` grows the same TI example into a radio that receives, sends, answers pings and starts them, driven by text commands on its serial port (list at the top of the file). Build it like `rfrx.c`, with the protocol chosen by `phyType868` in the SysConfig file; for the Wi-SUN settings add `#define RX_ADV` and the command list shown above. With it, the CC1407P on the LP-MSPM0G5187 and this board exchanged packets in both directions on 2-GFSK 50 kbps and all eight Wi-SUN FSK modes: 20 of 20 each way in seven of them, with losses toward the CC1407P in modes 4a and 5, and pings partial in four modes. The table is in the [sender README](../../mspm0g5187/radio/README.md#two-radios-the-cc1407p-against-a-cc1354p10).

Three details made it work, all in `partner.c`. With TI's 802.15.4g settings, this radio sends and stores each payload byte bit-reversed against the CC1407P, so `partner.c` reverses bytes both ways. The receive command's `maxPktLen` is an 8-bit field in `CMD_PROP_RX`, so 255 is the most it takes. And the Wi-SUN transmit header carries the length plus the 4-byte checksum, low byte first, with the whitening bit set as the CC1407P sends it.

## If you see this

- **DSLite fails with `Error -242 @ 0x0 A router subpath could not be accessed`.** Suspected Tool or docs: a program was already running and its one debug connection had been used, here by a flash backup read. Unplug the board's USB for 3 seconds and flash again.
- **Nothing on the serial port after flashing.** Run `xds110reset -s <probe serial>`; the image does not start on its own after DSLite.
- **Lines missing from the printout.** The serial bridge drops lines while no program reads the port, and replays old ones when it is opened. The `count ok=` line is the radio core's own count; the first line after opening can be a stale one.
- **Wi-SUN packets arrive with every byte backwards** (0x62 where the text has 0x46, `F`). With TI's 802.15.4g settings the radio stores each byte bit-reversed, and the two-byte header low byte first (`08 40` is length 64). Reverse the bits of each payload byte. The checksum is checked by the radio either way.

All four are in the [CC1354P10 FAQ](../FAQ.md), with the GitHub SDK libraries, and so is the O-QPSK result.

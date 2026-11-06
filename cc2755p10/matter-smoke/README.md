# Matter over Thread Smoke CO Alarm on the LP-EM-CC2755P10

Terms: [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

Status: Works. The image builds from TI's Matter release, boots on the board, exposes the Matter Smoke CO Alarm device type (0x0076, cluster 0x005C) and changes state from a UART line. Joining a Thread network and Apple Home did not run: they need a phone. The data-model and UART results below are from one run.

## What it is

TI's Matter release `matter-v1.5-ti-1.2-ea.2` (tag, commit `33a45a1b`) ships only the lighting app for the CC27xx (`examples/lighting-app/ti/cc27xx`). Its smoke example (`examples/smoke-co-alarm-app`) has targets for two other vendors only. The data model (`smoke-co-alarm-common`, ZAP file) is chip-neutral, so this port is TI's lighting shell with the smoke data model and three files from this repo:

| File (in `overlay/src/`) | Does |
|---|---|
| `SmokeAlarm.cpp/.h` | Sets smoke and CO state (critical or normal) in the cluster, recomputes the expressed state, reads `SMOKE 1`, `SMOKE 0`, `CO 1`, `CO 0` lines from the XDS110 backchannel UART at 115200 and answers `FRL OK` |
| `LightingManager.cpp/.h` | Stand-in for TI's light so `AppTask.cpp` compiles unchanged: left button (BTN-1) sets smoke critical, right button (BTN-2) short press clears both |
| `ZclCallbacks.cpp` | Identify callback only |
| `patch_apptask.py` | Two-line patch to a copy of TI's `AppTask.cpp` (drops the OnOff include, starts this repo's code). TI's source is not copied into this repo |

The UART is the one TI's logger already opened, so reads go through `uartConsoleRead` (a second `UART2_open` returns NULL). The logger shares the port: the Matter log scrolls on it, so a reply can land between log lines.

## Build (in Docker on a Mac, about 6 minutes warm)

`build.sh` runs inside `ubuntu:22.04` (amd64) on the volume `frl-matter-build` that holds TI's tree and toolchains (public sources, see `cc35x1/matter/` for how that volume is made). Phases:

    ./build.sh sub      # fetch the cc27xx submodules: F3 SDK (1.3 GB) and ot-ti (2.8 GB), 4 min
    ./build.sh sdk      # build the F3 SDK libraries (rcl and others), 2 min
    ./build.sh smoke    # the app: 5 min 50 s cold gn+ninja
    GN_EXTRA='chip_enable_icd_server=true chip_openthread_ftd=false' OUTN=smoke-icd ./build.sh smoke   # sleepy end device variant (links, not flashed)

The images land in the build volume's `out/<name>/` folder. Remove the added source with `docker run --rm -v frl-matter-build:/b alpine rm -rf /b/matter/third_party/ti_simplelink_sdk/repo_cc23xx_cc27xx /b/matter/third_party/openthread/ot-ti /b/matter/examples/smoke-co-alarm-app/ti` (about 4 GB).

## Flash (read first)

A raw ELF flash can leave debug locked (the build's CCFG differs from the board's known-good one: a bootloader word of 0 instead of 0xFFFFFFF0, and checksum and user-record fields). `make_flash_hex.py` here is `cc2755p10/thread/make_flash_hex.py` with one allowed difference (the bootloader word, bytes 0 to 3, zero in the ELF; the board keeps its known-good value 0xFFFFFFF0 because OTA is off and the app's vector table sits at flash 0). It writes a `.hex` with the board's own CCFG and SCFG:

    python3 cc2755p10/matter-smoke/make_flash_hex.py chip-LP_EM_CC2755P10-smoke-co-alarm-example.out backup-ccfg-1.bin scfg-sdk-default.bin smoke-flash.hex
    /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2755P10.ccxml -e -f -u smoke-flash.hex      # 28 s

## Use

- Console and input: `/dev/cu.usbmodem<probe serial>1`, 115200 8N1. `python3 cc2755p10/matter-smoke/latency_uart.py <port> 20` times 20 lines.
- Pairing: Home app, Add Accessory, scan `setup-qr.png` or enter 3497-011-2332 (passcode 20202021, discriminator 3840, vendor 0xFFF1, product 0x8005, TI's test values; the board prints them at boot). After the first 5 minutes of a boot press BTN-2 for 2 seconds to open pairing again. Expect an uncertified-accessory warning.
- Restore the board to its shipped image from the backup you took before the first flash, with `cc2755p10/restore.sh` (see [AGENTS.md](../AGENTS.md)). Flashing anything over this image also erases the Matter pairing (the fabric lives in the NVS region at 0xE0000).

## Known limits

- RAM is 99 percent allocated (static `.bss` 154 kB of 162 kB, of which the FreeRTOS heap is 44.9 kB); run-time heap headroom was not measured.
- The data model includes the ICD Management cluster; the router build answers its two Check-In commands as unsupported.
- Test attestation credentials only. Not certified.

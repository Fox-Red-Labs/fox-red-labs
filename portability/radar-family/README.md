# What changes to move the IWRL6432BOOST people counter to another chip of the family?

On mmWave L SDK 05.05.04.02, the firmware for all five chips (IWRL6432, AWRL6432, IWRL6432AOP, IWRL1432, AWRL1432) builds unchanged from TI's presence and tracking demo. The host code needs no change either. What changes is the cfg: one line on every chip, three more on the AOP part, one on the 1432 parts. All four builds and their boot images were made on a Mac on 2026-11-03. None has been flashed, so every row below is "Should work".

Terms: [vocab](../../docs/vocab.md). AOP means antenna on package. "Variant" means one of the SDK's per-chip project folders. 1 kB = 1,024 bytes.

## The five chips

The IWRL6432 and AWRL6432 share one build and one board: TI's user guide SWRU596 covers the "IWRL6432BOOST/AWRL6432BOOST EVM". The IWRL1432 and AWRL1432 also share a build. The AWRL parts are the automotive grades (AEC-Q100, junction up to 125 °C). All five have a 160 MHz M4F, the HWA 1.2 accelerator at 80 MHz and 1 MB of RAM (ti.com product pages, read 2026-11-03).

| | IWRL6432 / AWRL6432 | IWRL6432AOP | IWRL1432 / AWRL1432 |
|---|---|---|---|
| Band (ti.com) | 57–64 GHz | 57–63.5 GHz | 76–81 GHz |
| SDK variant | `xwrL64xx-evm` | `xwrL64xx-aop` | `xwrL14xx-evm` |
| Define | `-DSOC_XWRL64XX` | `-DSOC_XWRL64XX` | `-DSOC_XWRL14XX` |
| SysConfig | `--device XWRL64XX --package FCCSP` | `--device XWRL64XX --package AOP` | `--device XWRL14XX` |
| Libraries | `*.xwrL64xx.*` | `*.xwrL64xx.*` | `*.xwrL14xx.*` |
| RF patch in the image | `rfsfirmware/xWRL6432` | `rfsfirmware/xWRL6432_AOP` | `rfsfirmware/xWRL1432` |
| Linker file, `main.c` | same | same | same |
| cfg lines that change | `trackingCfg` | `trackingCfg`, `chirpComnCfg`, `compRangeBiasAndRxChanPhase`, `antGeometryCfg`, adds `steerVecCorr` | `trackingCfg`, `chirpTimingCfg` |
| Host parser (`iwrl6432boost/live/`) | no change | no change | no change |

The SDK also has a WCSP variant (`xwrL64xx-wcsp`, adds `-DSOC_64XXWCSP`, patch `xWRL6432_WCSP`). It builds the same way.

Other differences sit inside the SDK, and you don't touch them:
- The 1432 SysConfig file has no flash-size line, keeps a different low-power RAM mask, and has no timer callback.
- The demo scales frequency by 4 on the 1432 and by 3 on the 6432 (`motion_detect.c`).
- The AOP firmware detects the package at run time (`SOC_isDeviceAOP()`) and switches its CLI strings.

### What the host sees

Every build writes platform `0xA6432` into the frame header (`motion_detect.c`, line 843), the 1432 included. The TLV ids our parser reads (1, 301, 308, 309) are the same on every variant. So the same host code reads every chip, and it can't tell them apart from the stream.

## The cfg changes

`port_cfg.py` makes them. Our `people-count-250ms.cfg` ported to each chip is in `cfg/`.

1. **Every chip on 05.05: `trackingCfg` keeps four values.** Our line is the 05.04 form, `trackingCfg 1 2 100 3 61.4 191.8 250`. On 05.05 the fifth value is `boresightFilteringEnable`, and the firmware works out velocity limits and frame time from the chirp itself (`utils/tracker_utils.c`). Sent as it is, `61.4` turns boresight filtering on and the rest is ignored, with no error. The line becomes `trackingCfg 1 2 100 3`. This applies to the IWRL6432BOOST too if it ever runs a 05.05 image; today it runs TI's 05.04 Presence_Demo, so the live cfgs stay as they are.
2. **AOP:** MIMO goes from BPM to TDM (`chirpComnCfg` value 5, 4 → 1). The antenna map becomes TI's AOP map with 2.5 mm spacing: `antGeometryCfg 1 1 1 0 0 1 1 3 1 2 0 3 2.5 2.5`. All six receive phases become +1, and `steerVecCorr 1 1 1` is added. Each value comes from TI's `xwrL64xx-aop` profiles.
3. **1432 parts:** the chirp start moves up 17 GHz (`chirpTimingCfg ... 60.5` → `77.5`). Our sweep is 28 µs × 40 MHz/µs = 1.12 GHz, so the 1432 sweep is 77.5–78.62 GHz, inside 76–81 GHz.

**Unknown: 1432 antenna spacing.** TI's own 1432 profile keeps 2.418 mm, the 6432 value. Half a wavelength at 78 GHz is 1.92 mm. SWRU596 page 10 gives λ/2 at 62 GHz for the 6432 board. Range and speed don't depend on spacing, but angle and position do, so they can't be trusted on a 1432 board until someone measures them.

## One-command build

```
bash portability/radar-family/build/build.sh /tmp/radar-family
```

It copies TI's demo, builds the four variants with `make`, then makes the boot images in two Debian containers:
- `build/rprc.sh` runs `out2rprc.exe` under mono on native arm64, because mono crashes under amd64 emulation.
- `build/appimage.sh` runs TI's x86-64 image tools on amd64.

It took about 20 s of `make` per variant, plus the container pulls, on 2026-11-03. It doesn't flash anything.

## Check a cfg before you send it

```
python3 portability/radar-family/cli_check.py ~/ti/mmwave_l_sdk_05_05_04_02/examples/mmw_demo/motion_and_presence_detection/source <file.cfg>
python3 portability/radar-family/port_cfg.py iwrl6432boost/live/cfg/people-count-250ms.cfg iwrl1432 out.cfg
```

`cli_check.py` reads the demo's command table and each handler's argument test, then flags any line the firmware would reject or would cut short without an error. It evaluates those tests with a small whitelist of numbers, comparisons and `and`/`or`/`not` (no `eval`). A count it can only know at run time is a note, not a failure.

Results on 2026-11-03:
- TI's 05.05 profiles all pass.
- Our four live cfgs each fail on `trackingCfg` alone.
- Ported to the three variants, all 12 pass, and so do the five files in `cfg/`.

Both tools have `--selftest`.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Variants that build | 4 of 4 | SDK 05.05.04.02, Mac, TI Arm Clang, SysConfig 1.28.0 | measured | `portability/matrix.csv` X59–X63 |
| Boot image, `xwrL64xx-evm` | 253.3 kB | `build/build.sh`; TI's prebuilt is 253.5 kB | measured | `data/results/iwrl6432boost.csv` |
| Boot image, `xwrL64xx-aop` | 253.9 kB | TI's prebuilt is 254.1 kB | measured | `data/results/iwrl6432boost.csv` |
| Boot image, `xwrL14xx-evm` | 250.7 kB | TI's prebuilt is 250.9 kB | measured | `data/results/iwrl6432boost.csv` |
| Boot image, `xwrL64xx-wcsp` | 253.6 kB | TI's prebuilt is 253.8 kB | measured | `data/results/iwrl6432boost.csv` |
| Live cfgs that pass `cli_check` on 05.05 | 0 of 4 | all fail on `trackingCfg` only | measured | this page |
| Ported cfgs that pass | 17 of 17 | 4 cfgs × 3 variants, plus the 5 in `cfg/` | measured | this page |

Each of our images is 192 B smaller than TI's prebuilt, and its header has the same layout. Nothing here ran on a radar.

## If you see this

- If the tracker reports odd speeds or ignores the boresight filter after a cfg that worked on 05.04, look at `trackingCfg`. Keep four values on 05.05.
- If the build stops with `'mkdir' is not recognized` or a `\` path error, pass the Unix helpers the way `build/build.sh` does (`"MKDIR=mkdir -p"` and the rest).
- If `out2rprc` aborts in the container with a mono stack trace, the container is amd64. Run that stage with `--platform linux/arm64`.

## Where these facts come from

- MacBook (Apple silicon), macOS 26.
- MMWAVE-L-SDK 05.05.04.02, TI Arm Clang, SysConfig 1.28.0, CCS 20.1, Docker with `debian:bookworm-slim`. Checked 2026-11-03.
- TI user guide SWRU596, https://www.ti.com/lit/ug/swru596/swru596.pdf.
- TI product pages for the five chips (#params).
- Records: `portability/matrix.csv` rows X59–X63, `data/results/iwrl6432boost.csv`, `../../iwrl6432boost/AGENTS.md`, `../../iwrl6432boost/FAQ.md`.

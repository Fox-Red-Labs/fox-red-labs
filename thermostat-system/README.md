# MCU-only thermostat on an LP-MSPM0G3519, IWRL6432BOOST radar and LP-EM-CC35X1

Three boards and four wires: the IWRL6432BOOST radar sends its frames over a UART to an LP-MSPM0G3519 that draws the thermostat screen, and the G3519 sends one status line a second to an LP-EM-CC35X1 that publishes it over Wi-Fi to a broker, which posts it to Home Assistant. The screen half Works on live radar data (see the experiment). The direct radar wire and the radio leg have not run end to end (see Limits).

Terms: [vocab](../docs/vocab.md). Experiment: [MCU-only thermostat](../docs/experiments/2026-10-10-thermostat-mcu-only.md). Wire format: [PROTOCOL.md](PROTOCOL.md).

**What is here, and what is not.** The folders below hold the sources and scripts for the screen and radio halves and `PROTOCOL.md` (the wire format). Not here: TI's SDK, CCStudio and the compilers (you install them), built images, the radar's capture files, and the home network's name and password, which go into the radio firmware copy you build outside the repository. Take it alone with `git sparse-checkout add thermostat-system tools third-party/lvgl mspm0g3507/lvgl-g3507 am13e230 data`.

| Folder or file | What it is |
|---|---|
| `g3519/` | Screen firmware: `build.sh` (one command), `therm.syscfg`, `src/therm.c`, Barlow fonts and the logo made by `tools/make_assets.py` |
| `g3519-zephyr/` | Zephyr 4.4.2 plus LVGL footprint probe, see its README |
| `cc35x1/` | Radio firmware: `build.sh` patches TI's `mqtt_client` (needs `HOME_SSID`, `BROKER_HOST`), `README-part.md` |
| `tools/replay.py` | Replays a radar capture into the G3519's USB serial port and logs frame to screen time |
| `tools/hub.py` | MQTT broker on the Mac that posts to Home Assistant by REST and logs latency |
| `tools/latency.py` | Joins the replay and hub logs per frame |

Ten-minute path for the screen half: `g3519/build.sh`, flash `g3519/out/therm/therm.out` with DSLite, run `python3 tools/replay.py <capture.bin> <serial port> out.csv`. The home network's name and password go only into the radio firmware copy built outside this repo.

## Screen

The screen uses the Fox Red Labs palette (Paper background, Ink text, Fox Red only for the presence badge and logo, Ink-outline buttons), with the horizontal logo in the header (120x59 image, `g3519/src/frl_logo.c`) and Barlow 14, 20, 48 and 64 px fonts. It has two pages, a thermostat page and a Sensors page, and shows the SHOWN and STAT lines of [PROTOCOL.md](PROTOCOL.md).

With the radar live the screen refreshes 6 to 8 times a second with no drops. Size: 191.7 kB of flash and 43.7 kB of RAM (bss), which includes the 28 kB LVGL heap. 1 kB = 1,024 bytes.

## If you see this

| What you see | Cause | Do this |
|---|---|---|
| The board prints its banner and a start-up line, then no `STAT` lines | The LVGL heap is too small: the two-page screen peaks at 14.9 kB, and a 12 kB heap runs out and hard-faults in `lv_draw_fill_dsc_init` | Build with `g3519/build.sh`, which defaults to a 28 kB heap (`MEM_KB=28`) |
| Drawing an image crashes | The LVGL slim draw patch removes the image decoder | Keep `LV_DRAW_SW_SLIM 0` and `LV_USE_IMAGE 1` in `lv_conf.h` |

## Limits

The direct radar-to-G3519 wire and the CC35X1 radio leg have not run end to end. Until the wire is fixed, a Mac script sends the radar its configuration and forwards frames to the G3519 over USB.

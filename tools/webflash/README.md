# LaunchPad bench: use a board plugged into this computer

Pull the private repository, run the local server, then open the bench in Chrome or Edge. The browser talks to USB devices on this computer. Existing demo images are included in the checkout.

From the repository folder:

```sh
git pull --ff-only
```

On Windows with Python installed:

```sh
py tools/webflash/serve.py --board mspm0g3507
```

On macOS or Linux:

```sh
python3 tools/webflash/serve.py --board mspm0g3507
```

Open <http://localhost:8765/site/flash/bench/?board=mspm0g3507>. Keep the terminal open. Ctrl+C stops the server. If port 8765 is already used, add `--port 8766` and open the address it prints. The server listens only on this computer.

Choose **Connect a board**, then the board's **XDS110** in the browser chooser. The bench reads its chip identity and loads that board's demos. Choosing another board in the preview changes only the information shown; it never overrides chip detection or selects a device for writing.

**Boards the bench has never seen (open aperture, 2026-10-17).** `python3 tools/webflash/sdk_catalog.py` reads CCStudio's GEL scripts (the DEVICEID of every MSPM0 part and the CC23xx and CC27xx families) and the kit folders in the MSPM0 and SimpleLink F3 SDKs, and writes `site/flash/bench/boards-predicted.json`: one read-only entry per kit TI describes that no run here has touched, with its ID, flash and RAM, LED pins, whether it programs with ECC, and the file each fact came from. The generator must reproduce the five tested MSPM0 entries first or it writes nothing; `build_page.py` runs it, and `site_check.py` fails when the file is stale. On the page a board is one of three things: **Tested here** (a run is recorded), **Predicted from TI's files** (named, read, its flash size checked against the SDK through the chip's own size register, never written), or **Recognised** (named only). A predicted board climbs to tested only through the three hardware rungs in `docs/research/bench-device-ids-2026-10-17.md`. A CC2340R53 is its own entry (it used to be named as the R5), and the page reads PARTID through TI's configuration port (offset 0x4, from CCStudio's register map; checked on the LP-EM-CC2340R5: variant byte 0x3B).

**No-code controls (System Lab step 3, reworked 2026-10-17).** The LP-MSPM0L1306 has a "Pin, timer, bus" card that reads like the Motor Lab's console: a plus adds one row (a pin to read or drive, a timer toggle with on and off times, a PWM at 100 Hz to 20 kHz and 0 to 100 percent duty, a bus scan, or a register read at an address), each row shows its settings and the line of C it stands for, a minus removes it, and the board drawing lights the chosen pin with its setup (`mspm0l1306/img/header-positions.json`). A plugged-in BoosterPack picked from `boosterpacks.json` (built by `build_boosterpacks.py` from data/parts.json and docs/boards) adds its own rows, labelled predicted because none is on this board. One program (`player/controls.c`, built by `build_controls.py` from TI's I2C controller example) applies an 84-byte table the page writes to the last flash sector (one pin slot, four register reads, a scan) and reports through a status block in RAM. A toggle is timed on the pin's output register, a driven pin is read back from the pad, and a PWM is judged through its timer (load and compare registers, enable bit, counter samples), because the GPIO input register does not follow a timer-driven pad. Run record, timings and what is proven or predicted: `mspm0l1306/bench/controls/README.md`; the labels on the card come from `controls-proven.json`.

The picker covers all 22 inventory boards: 10 have USB chip identification and eight have firmware-writing browser demos. Other entries link to their existing board notes and instructions. These routes differ by board. AM13E230 remains blocked on starting its program, and CC35X1 uses its existing desktop-tool instructions. No new flashing method is inferred from a board's name or USB serial number.

For G3507, blink and colour-player firmware builds and software checks pass. Flashing, visible LED behavior and restore still need a run on the physical board. The page says this before writing and saves the main flash first. Keep the downloaded backup.

If the board is unknown, the page stops with nothing written. It does not switch to another connected probe. Check the printed board name in the picker and use its instructions. If the browser reports no USB support, use Chrome or Edge at the localhost address above; opening the HTML file directly is not the launch method.

If the board picker is empty and Connect does nothing, the page has not finished loading. Use the local server command above, not a `file:` address. The page now shows a loading error if a script or data file fails. Share that error and the page address if it persists. The server sets JavaScript content types explicitly, including on Windows.

**What the bench does, and what it does not.** It saves the board's flash, writes a demo through the XDS110, starts it, checks the result (an LED pin, values in the chip's memory, or serial output) and puts the saved copy back. It is not a source debugger: no breakpoints, stepping, register view or memory view. Use CCStudio, or GDB with OpenOCD, for those.

**When something goes wrong.** Each failure names the step and the next action. If the cable comes out while the page writes, the page says the board may hold a partly written program and that Put back with the saved `.json` recovers it; Put back compares first, so pressing it again after a second cut writes only what still differs. If another program holds the probe, the page says so. "Download a diagnostic record" saves the steps of the visit as JSON for a report: no flash contents, Wi-Fi names or passwords, addresses or full probe serial numbers. Open the file before you send it.

**Tests with no board.** `node --test tools/webflash/*.test.mjs` runs every engine and page test. `sim-xds110.mjs` is a fake XDS110 with an MSPM0-like chip (CMSIS-DAP over a WebHID-shaped device, flash controller, the page's RAM loader and CRC routine, injectable faults); `sim-xds110.test.mjs` drives the engine through save, write, check, put back, a lost answer and an unplug part way. `dlab-sim-run.mjs` runs the real page in headless Chrome against it (`NODE_PATH=<folder with playwright-core> node tools/webflash/dlab-sim-run.mjs --out <folder>`; it starts `serve.py` itself): chooser closed, WebHID blocked, probe held, unknown chip, the whole path, unplug and replug, and the diagnostic record. A pass is a software result only; it does not show that a real board, USB stack or chooser works. `python3 tools/site_check.py` runs the node tests except the two that need a board or server.

**Browser run on a real board, no chooser (2026-10-22).** `tools/webflash/dlab-browser-run.mjs` drives the bench in headed Chrome. A Chrome device policy (`WebHidAllowDevicesForUrls`, set with `defaults write com.google.Chrome`, for the page's own origin) grants the XDS110, and the page's `?auto=1` link flag makes Connect take the allowed probe with no chooser (`autoPick` in `boards-list.js`; a reader's page never skips the chooser). WebHID shows no serial, so the run limits what `getDevices()` returns to the probe under test. Web Serial is a stand-in over serialport, because Chrome rejects the Serial allow policy at the recommended level that `defaults write` gives. Take the probe lock, run `node tools/webflash/dlab-browser-run.mjs --probe <serial> --scenario l1306|two|printloop --out <folder>`, and delete the policy afterwards. An LP-XDS110's page serial (CMSIS-DAP) differs from its USB serial; pass `--also-dap` for it.

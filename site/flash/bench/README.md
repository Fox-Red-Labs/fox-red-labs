# Digital Lab bench: data and image formats

How to run the page is in `tools/webflash/README.md`. This file is for whoever adds an image or a demo: the radio images, the next board, a new model.

## Best demos first

`best.json` lists, per board, the demo ids (ids in `recipes.json`) to show first when that board is in hand, best first: measured here, fits the chip, looks good. `bestFirst()` in `stage.js` applies it; the rest follow in their usual order. `tools/webflash/ai.test.mjs` fails when an id does not exist for its board.

## Pick an AI model and see how it does

The models are images, one per model (BitNetMCU picks the model when it is compiled). A manifest with a `digit` or `occ` block makes `build_page.py` give the recipe an `ai` entry (`tools/webflash/ai_recipes.py`): the model's facts, the size of the image the page writes (from its link map), and our measured rows for that board with the row each came from. The page shows only the board in hand and links once to `edge-ai.html` for other chips. A number with no row says "no row for this board yet"; nothing is typed in twice.

| Family | Images | Input | Mailbox |
|---|---|---|---|
| `digit` | `digit` (cnn_64), `digit-cnn_16small`, `digit-12k`, `digit-1k` (`tools/webflash/build_digit.py`) | a 16 x 16 int8 drawing | `dg` in `player/digit.c` |
| `occ` | `occ-500`, `occ-2k`, `occ-8k`, `occ-30k` (`tools/webflash/build_occ.py`; 30k needs more than 4 kB RAM, so not on the L1306) | one of 20 stored 4 x 32 windows (light, sound, CO2, motion), picked by number | `oc` in `player/occ.c` |

Both mailboxes follow one pattern: a magic word when the chip has finished its own self-test, the CPU clock, the self-test result, then `seq_in` (the page bumps it) and `seq_out` (the chip copies it when the answer is ready) around the input and the answer. The self-test is what the chip ran at start: ten stored digits, or twenty stored windows with the reference class and logits. The address of the mailbox is read from the symbol table at build time and written in the manifest.

## Change: the shared parameter block (format 1)

Every image that can be changed from the page holds its settings in the board's last flash sector (the sector the colour player's table also uses; `table` in `tools/webflash/boards_mspm0.py`). The page rewrites only that sector, then resets and runs the program. No compiler runs and the program is not written again. One source defines it: `tools/webflash/params.py`.

Little-endian 32-bit words:

| Word | Content |
|---|---|
| 0 | magic `0x50524C46` |
| 1 | format version, `1` |
| 2 | count of values that follow |
| 3 | image id (a number per image kind: blink 1, breathe 2, thermometer 3, digit 4, occupancy 5) |
| 4 and on | one unsigned value per parameter, in the order of the manifest's list |

The program calls `frl_get(i)` from `tools/webflash/player/frl_params.h`. It returns the value written for parameter `i`, clamped to the parameter's limits, or the default when the magic, version or image id is wrong, the block is shorter than `i`, or the sector is erased. `build_board.py` writes `frl_params_gen.h` (image id, count, defaults, limits) from `params.py`, so the numbers exist once.

The manifest carries the same list as `params`:

```json
"params": {"kind": "params", "format": 1, "addr": "0x0007FC00", "magic": "0x50524C46", "image_id": 1,
           "items": [{"id": "on1", "label": "LED on, part 1", "unit": "ms", "min": 10, "max": 5000, "default": 150, "step": 10, "note": ""}]}
```

`build_page.py` copies it to the recipe. The Change tab draws one input per item, clamps what is typed (`paramsClamp` in `stage.js`), writes `paramsWords()` at `addr` through `Mspm0.writeSector()` (erase, program, read back), runs the program, and checks what the kind allows: a blink pattern is timed on the LED pin; the digit and occupancy readers are checked by their magic word. A recipe without `params` has nothing to change from the page, and the Change tab says so.

To add an image: write its program with `frl_get`, add its kind to `params.py`, pass `params_kind` to `build_board.build()` and `params.manifest_block()` to `build_board.write()`. Blink patterns longer than 8 values (SOS) carry no block. The colour player keeps its own sequence table (magic `0x52434C46`), which is a list of colour and time pairs and not a list of numbers; the manifest's `table` block describes it. The LP-MSPM0L1306's TI blink image has no block yet.

Image limit: an image with a block must end below the last sector (`write()` checks it).

## Go deeper

One short line under the stage: the repository, the board's folder, its `AGENTS.md`, `docs/builds` and `prebuilt`, all in the reader copy (`github.com/Fox-Red-Labs/fox-red-labs`). The board folder comes from `kit.json`.

## Rebuilding the images

```sh
export FRL_SYSCONFIG=/Applications/ti/sysconfig_1.28.0   # only if the validated 1.26.3 is not installed; manifests then say validated: false
python3 tools/webflash/build_digit.py        # every digit model, three boards
python3 tools/webflash/build_occ.py          # every occupancy model that fits
python3 tools/webflash/build_sensor.py       # thermometer
python3 tools/webflash/build_board.py mspm0g5187 --only-blink
python3 tools/webflash/build_examples.py mspm0g3519 --only breathe
python3 tools/webflash/build_page.py         # recipes.json and recipes/
```

Tests: `node tools/webflash/ai.test.mjs`. On a board: `tools/webflash/ai-run.mjs` (node-hid, no browser) and `dlab-browser-run.mjs --scenario ai` (real Chrome).

## Pin panel

`pinouts.json` (written by `tools/webflash/build_pinouts.py`) lists every pin of each MSPM0 kit's chip package. The package is the `--package` line of TI's `gpio_toggle_output.syscfg` for that kit in the MSPM0 SDK; the pin numbers and names are the package table of SysConfig's device data. The page marks the pins the demo's own record names (`usedPins` in `stage.js`: its LED and button, wiring table, colour pins, and the console jumpers of a serial demo). It adds no names.

## Radio boards and the CC35X1

A recipe with a `hex` file runs through `goCc` (SACI write, the chip's CRC32, reset and run through `Saci.close()`), then its own check: a `status` block in RAM must show its counter rising, a `uart` demo is read in Talk to it. A change on these boards writes the whole image again with the settings sector filled (`imageWithParams`, then `program`). A put-back from the three saved `.bin` files uses `putBack` after `readBins`. Settings ids 6, 7 and 8 (blink, message period, button repeat) are in `params.py`.

The CC35X1 panel (`ota-ui.js`, data `site/flash/cc35-ota.json`, driver `site/flash/cc35-ota.js`) types TI's OTA example menu over Web Serial: connect, scan, join, server, download, install, restart, keep or go back, and the waiting-image menu. The Wi-Fi password and the server address stay in their boxes and are never stored or logged. The page does not write that board's flash.

## What leaves the page as a file

`export-safe.js` holds the rules, tested in `tools/webflash/export-safe.test.mjs`. A CSV cell from the board that starts with `=`, `+`, `-` or `@` gets a leading apostrophe, so a spreadsheet shows it as text instead of running it; plain numbers, negative ones too, stay numbers. The console keeps the last 50,000 pieces of text of a visit and the live values the last 200,000 readings; a save made after the oldest were let go says how many are missing. The screen shows the last 5,000 pieces of console text. The copy behind "Put back" and the `.bin` is always the board saved last, even when its flash matched a copy saved earlier in the visit, and the `.bin` name starts with the kit name.

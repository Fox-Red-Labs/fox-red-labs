# Porting trick map

Terms: [docs/vocab.md](../../docs/vocab.md).

What it is: a table of proven replacements. Point it at a project written for one chip and a target board, and it lists the calls the project makes, what each one becomes on the target, the trap that cost time on a real port, and the check that proves the port still works. Every entry came from a port run on a real board in this repository.

The map has three layers, so a new source API or a new target costs one file, not one file per pair:

- `idioms.json`: vendor-neutral names for what code is trying to do (`uart.rx_until_idle`, `dma.circular_halfcomplete`, `gpio.input_irq_edge`). Each says what a check must confirm.
- `apis/<api>.json`: a vendor call pattern mapped to an idiom (`LL_USART_EnableIT_IDLE` is `uart.rx_until_idle`). Maps: `stm32-hal`, `stm32-ll`, `cmsis`, `freertos`, `arduino-esp32`, `nimble-arduino`, `esp-idf`, `cmsemicon`, `mspm0-driverlib`, `ti-drivers`.
- `impl/<family>.json`: how a target does an idiom. Difficulty is `direct` (rename), `adapter` (a small wrapper), `redesign` (a different model; the tested pattern) or `none` (no equivalent; what to do instead). Each entry carries the trap, what the port costs (`degrades`), the check, the proof (port id, board, level, file) and the glue code to reuse.

Status words: `proven` has a proof that ran or measured on a board; `built` compiled and did not run; `reported` is from a vendor document with no run and is never shown as proven. A pair map (STM32 HAL to MSPM0, ESP32 to CC2340R5) is joined from the three layers by the tool, never written by hand.

## The three commands

```bash
python3 tools/port/tricks.py map  <project folder> --to <board id>             # each call: idiom, difficulty, replacement, trap, proof; "no entry" for the rest
python3 tools/port/tricks.py pack <project folder> --to <board id> --out DIR   # DIR/PORTING-PACK.md, for an assistant to read before the port
python3 tools/port/tricks.py learn --port <port id> --src <project folder>     # after a port: calls with no entry, idioms with no implementation, entries to upgrade
```

`python3 tools/port/port.py plan <project> --target <board> --out DIR` writes the same pack beside the plan. `python3 tools/port/tricks.py --check` validates the data and runs inside `tools/site_check.py`.

Coverage is honest on purpose: a call with no entry is listed as "no entry", and an idiom the target has no tested implementation for says so. Instance method calls in C++ (`pServer->createCharacteristic`) are counted only when the method name is listed in the API file.

## How an entry gets added

1. Run a port on a board and keep the check that passed (a row in `data/results/<board>.csv`, a log or a source file in the port's folder).
2. `tricks.py learn --port <id> --src <project>` lists the calls the map did not know.
3. Add each call to its `apis/<api>.json` with an idiom (add an idiom to `idioms.json` only when no existing one fits) and a `from` that says where the call was seen.
4. Add or upgrade the target implementation in `impl/`: the replacement, the trap that cost time, what the port costs, the check, and a `proof` entry (`port`, `board`, `level`: `measured`, `ran` or `built`, `what`, `path`, optional `results` as `board:test`). The path must exist and the port must be in `site/data/ports.json`.
5. `python3 tools/port/tricks.py --check` must print OK.

No proof, no `proven`: write `status: reported` with a `cite` for the document, and it stays a lead. Keep it supplier-neutral: name the technique and the measured result, never copy text from a vendor guide. `ti_guide_silent` says in one line where a vendor migration guide is silent on a trap a port hit; it never quotes the guide.

## Port manifest (`ports/<port>/port.json`)

What it is: the one file that says how to build a port, run it on each bench board, and tell whether it still behaves like the original board. `python3 tools/bench/hil.py --port <id> [--board <b>]` reads it and prints PASS or FAIL with the difference from the original. `python3 tools/bench/portjson.py --check` validates every manifest and runs inside `tools/site_check.py`. Manifests exist for `stm32-usart-dma-rx-tx`, `esp32-ble-gamepad`, `stm32-bootloader`, `minmea` and `bitnetmcu`; a new port copies `ports/stm32-usart-dma-rx-tx/port.json`.

Placeholders in commands: `{py}` (the bench Python), `{port}` (the board's console device), `{baud}`, `{out}` (the run's output folder), `{board}`, `{repo}`.

```text
schema 1
id, paths[]            port id (the folder name) and the globs whose change triggers --since
source                 repo, commit, file (the SOURCE.md or README that says where the code came from)
original               the reference, captured on the original board before the port is judged
  board, captured      board id (a key of boards[], or set not_on_bench with the reason), date
  source               repo file holding the capture (check JSON, log, or the table the numbers come from)
  expect               what the original produced and every port must reproduce
    console            regex the console must show after the flash (banner)
    stdout_all[]       regexes the check's output must match
    json[]             {path, equals | regex} on the check's JSON
    capture            the recorded output itself (golden file or check JSON), kept for reading
  metrics{}            the numbers the original recorded, by metric name
check
  cmd, json            the host check (null: none), and the JSON file it writes
  exit0, repeat        exit status must be 0 (default true); runs per board, the metric is the median (the recorded numbers are medians of 3)
  own_lock             true when the check takes the probe lock itself; hil releases it first
  pass                 what passing means, in words
  metrics{}            how to read each metric: {json: "a.b"} or {regex: "...(group 1)..."}
  record{}             metric -> {test, variant} for the results row that --record appends
boards{}
  <board id>           key of tools/bench/boards.json
    build, image       build command and the image it makes (build null needs build_note)
    flash              how it is flashed, in words (the tool itself is the board's `kind` in boards.json)
    secs               console window after the flash; self_flash: the check script flashes the board
    protected          reason; the board is flashed only with --allow-protected
    budget[]           {metric, rule, pct | value}: rules min_pct_of_original, max_pct_of_original, min, max, info
```

Degradation budget: a rule compares this run's metric with `original.metrics`. Example: `burst_pps` at least 95 % of the original's 2259.4 packets a second, `burst_lost` at most 0. `info` is reported with its delta and never fails (the round trip includes the probe's USB bridge, so it is not the chip). A number that cannot be read fails its rule. Capture the original's expected output and numbers from a real run on the original board first; a manifest with `not_on_bench` says the original cannot be rerun and what its expected output rests on.

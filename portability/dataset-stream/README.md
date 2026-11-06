# dataset-stream: sensor samples from a TI LaunchPad to a Mac at a fixed rate

Terms: [docs/vocab.md](../../docs/vocab.md). A BoosterPack plug-in module is a board that plugs on top of a LaunchPad development kit. 1 kB = 1,024 bytes. Cause tags: **Tool or docs** (a tool or page is missing or wrong), **Our mistake** (something done wrong in this repo), **Chip limit** (the silicon cannot do it), **Unknown** (not found; "suspected" names the lead).

Board-independent C that takes one sensor reading at a fixed rate and prints it as one CSV line over the board's serial port, for `tools/dataset/capture.py` to record and label. The guide that uses it: [`docs/guides/collect-a-dataset.md`](../../docs/guides/collect-a-dataset.md).

Status (2026-10-08): host-tested only. **No board build exists and nothing has run on a board**: the BP-BASSENSORSMKII sensor BoosterPack plug-in module has not arrived. Layout follows `docs/repo-layout.md`.

| File | What it is |
|---|---|
| `core/ds_stream.c`, `.h` | Fixed-rate sampler (`ds_init`, `ds_due`), line and header formatting without printf, UART rate limit (`ds_max_rate_hz`) |
| `core/bmi160.c`, `.h` | Bosch BMI160 wake-up and read over any I2C, through three functions the board supplies |
| `test/host_test.c`, `test/run.sh` | Host test on the Mac: grid timing, skipped samples, counter wrap, exact line text, the BMI160 sequence against a fake bus |
| `boards/README.md` | What each board must supply, pin by pin |

Run the host test (no board):

```bash
portability/dataset-stream/test/run.sh
```

It prints 29 `ok` lines and `OK` (Apple clang on macOS 15.7.9, Intel, 2026-10-08).

Size for a Cortex-M0+, TI Arm Clang 4.0.4.LTS (the copy installed on this Mac; the GitHub compile job uses 5.1.1.LTS) at `-Os` (objects only, before linking): `ds_stream.o` 935 bytes of code, `bmi160.o` 316 bytes, no RAM of their own. `ds_due` uses one 64-bit division, which is expected to pull in the compiler's division helper at link time; that is not counted here and not measured.

## The loop a board runs

A sketch, not compiled or run on a board. `board_micros`, `uart_write` and `bus` come from the board file; `names` is the list of six channel names.

```c
ds_sampler s;
uint32_t last = 0;
char buf[64];
ds_init(&s, 100, board_micros());                 /* 100 samples a second */
uart_write(buf, ds_format_header(buf, sizeof buf, &s, "bmi160", names, 6));
for (;;) {
    uint32_t t_ms;
    if (ds_due(&s, board_micros(), &t_ms)) {
        int16_t v[6];
        if (bmi160_read(&bus, v) == BMI160_OK)
            uart_write(buf, ds_format_line(buf, sizeof buf, t_ms, v, 6));
        if (s.overruns != last) { uart_write(buf, ds_format_overrun(buf, sizeof buf, s.overruns - last)); last = s.overruns; }
    }
}
```

`t_ms` is the sample's scheduled time, so a late loop shows as a gap of whole periods, which `tools/dataset/check.py` counts. At 115200 baud a six-channel line of up to 52 bytes allows 221 samples a second in the worst case (`ds_max_rate_hz`); real lines are about 23 to 40 bytes. The core as written runs the BMI160 at 100 Hz with six channels; another rate or range means changing `ds_init`'s rate and the register values in `core/bmi160.c`.

## Questions

### Is there a board build I can flash?

No: the core is host-tested only, and each board needs a small board file (pins in `boards/README.md`). Not written: the sensor BoosterPack plug-in module has not arrived, so none has been written or run.

### Why is the code bigger than the 935 + 316 bytes listed?

Expected, not measured: `ds_due` uses one 64-bit division, and the Cortex-M0+ has no divide instruction, so linking should add the compiler's division helper. It was written with a 64-bit division. Nothing has been linked, so its size is unknown. A 32-bit version of `ds_due` would avoid it (not run).

### Why do the overrun lines start with #?

So `tools/dataset/capture.py` treats them as comments, not samples. It adds up the skipped count, and `check.py` finds the same gap from the `t_ms` values.

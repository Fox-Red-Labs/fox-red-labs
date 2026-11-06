# Post-quantum key exchange on a LaunchPad

A TI board and a Mac agree on a secret key with ML-KEM-768, a method meant to stay safe against quantum computers, then swap one encrypted line. The same code also runs on four more boards.

The page: [site/project-post-quantum.html](../../site/project-post-quantum.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## The key exchange with a Mac on the LP-CC1354P10-1

Host test, build, then load the board and watch the exchange over USB serial.

Status: Works. Boards: cc1354p10.

Folders this version uses (in a sparse clone, fetch them first, then read `cc1354p10/AGENTS.md`):

```bash
git sparse-checkout add cc1354p10 portability/pqc-join third-party tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-CC1354P10-1 LaunchPad development kit | 1 | makes the key pair and finishes the key |

### Before you start

- A Mac with CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, the SimpleLink CC13xx/CC26xx SDK 8.33.00.16 and OpenSSL 3.5 or later (`mac-setup/README.md`). The host test also needs Python with the `cryptography` package.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add cc1354p10 portability/pqc-join third-party tools mac-setup`.
- This board's debugger connects only just after power-on, so loading needs a USB hub port the Mac can switch with `uhubctl` (`cc1354p10/FAQ.md`).
- The run script backs up the board's 1 MB of flash first and writes it back at the end; it prints `READBACK MATCHES BACKUP` when the board is as it was.

### Steps

1. **Mac, terminal.** Build the board code for the Mac, run both sets of known-answer tests and the whole exchange against the Mac side. No board is needed.

   ```text
   portability/pqc-join/test/run.sh
   ```

   You should see: The last line is `pqc-join host test: OK`.

   If it stops on the OpenSSL version, install OpenSSL 3.5 or later and put it first on your path.

2. **Mac, terminal.** Build at -O3, the timed build. It touches no hardware and takes about 20 seconds once SysConfig has run.

   ```text
   cc1354p10/pqc-join/build.sh -O3
   ```

   You should see: The script ends with the path of the `.out` image.

   If SysConfig is not found, install it as `mac-setup/README.md` says and run the line again.

3. **Mac, terminal.** Set `UHUB` to your hub and port, and `RESTORE_IMG` to the `.out` you want left on the board afterwards. The run takes about two and a half minutes: 22 s to back up the flash, 18 s on the board, then restore and read-back.

   ```text
   UHUB=<your hub>:<port> RESTORE_IMG=<path to the .out to leave running> cc1354p10/pqc-join/run.sh -O3
   ```

   You should see: The console shows `board> KAT PASS`, then `key check  Mac d01a2f61  board d01a2f61  MATCH`, then `board> DECRYPTED (76 bytes, tag ok)` and the Mac's line. The script ends with `READBACK MATCHES BACKUP`.

   If the exchange stops at `join: making an ML-KEM-768 key pair`, the image did not start the random generator; build from this repository's `pqc_board.c`, which calls `TRNG_init()` before `TRNG_open()`.

### Expected result

The board and the Mac end with the same 4-byte key check and the board prints the decrypted line.

### What this shows

- ML-KEM-768 takes 20.8, 24.0 and 29.1 ms on a 48 MHz Cortex-M33; ML-DSA-44 verifies in 50.5 ms and signs in 154 ms on average.
- The board's own work in the exchange is about 73 ms; the wire at 115,200 baud takes about 420 ms.
- The same portable C, unmodified mlkem-native 2.0.0 and mldsa-native 2.0.0, runs on four more boards (the other rows below).

### Limits

- The exchange with a Mac ran on this board only. On the other four boards the algorithms ran and passed their vectors; no exchange was run.
- No protection against power or fault attacks is claimed. This is a demonstration of the math and its cost. Do not use it for real keys.
- Signing needs 50 kB of stack, so a board needs about 64 kB of RAM for ML-DSA-44; the LP-MSPM0G5187 (32 kB) runs ML-KEM-768 only.
- Signing time varies from 77 to 361 ms with the message, because a candidate signature is sometimes thrown away and retried.

### Numbers and sources

| | Value | Source |
|---|---|---|
| ML-KEM-768 at 48 MHz | 997k / 1.15M / 1.4M cycles for key pair, encapsulate, decapsulate | `data/results/cc1354p10.csv` |
| ML-DSA-44 at 48 MHz | 2.37M cycles key pair, 2,421,949 verify, 7,406,767 mean sign over 16 messages | `data/results/cc1354p10.csv` |
| Flash | ML-KEM 82.4 kB and ML-DSA 109 kB at -O3; 9.7 kB and 13.6 kB at -Os | `data/results/cc1354p10.csv` |

# Backyard Baseball '97 in ScummVM on the BeagleY-AI

Status 2026-11-10: **built, not run.** The game files are not on the board yet.

The Steam re-release (appid 3170540) is a bought copy; you need your own. Steam lists it for Windows only. It is a SCUMM HE game, so ScummVM can run it without Windows. Two things stopped it on the AM62L (`docs/experiments/2026-10-25-am62l-steam-box64.md`, ScummVM section):

| Stop | Cause | Change in `scummvm-baseball.patch` |
|---|---|---|
| Copy not recognised | The Steam files' `BASEBALL.HE0` MD5 `b855daca2b7c40fc45592cdf5ce54f86` is not in ScummVM v2026.4.0's table, which knows only `ff90541c...` | Add the MD5 as the same Steam Windows variant (`engines/scumm/scumm-md5.h`) |
| Assertion on `_scummStackPos` at Play Ball | The re-release's changed scripts pop below the bottom of the VM stack; the original program has no bounds check, ScummVM asserts (ScummVM ticket #15412, still open on v2026.4.0) | `push` and `pop` use a 256-slot shadow area below zero instead of asserting, and log the first five underflows (`engines/scumm/script.cpp`) |

## Build (on the board, 1,550 s)

In a Debian trixie arm64 container (`moonlight` on the board, `/root/gamestream` mounted at `/w`):

    git clone --depth 1 --branch v2026.4.0 https://github.com/scummvm/scummvm.git && cd scummvm
    git apply scummvm-baseball.patch
    ./configure --disable-all-engines --enable-engine=scumm,he --disable-debug --enable-release
    make -j4          # 28,014,144-byte scummvm binary

Packages: `build-essential pkg-config libsdl2-dev libsdl2-net-dev libfreetype-dev libpng-dev zlib1g-dev libjpeg-dev`.

## Next

1. Get the Windows files: sign in to Steam on a Mac or PC and run Steam's console `download_depot 3170540 <depot>` on that client (your own account; no script handles the password).
2. Copy to the board, `./scummvm --path=<game dir> --auto-detect`, play to Play Ball, read the underflow warnings.
3. If the shadow stack is wrong for this game, the warning count and positions say where; compare against the original program's behaviour.

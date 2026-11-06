#!/usr/bin/env python3
"""Turn an audio file into a Halloween-show track for the board, then rebuild the LP-MSPM33C321A image.

  python3 tools/add_track.py <audio file> [--name NAME] [--start S] [--length S] [--title TEXT] [--no-build]
  python3 tools/add_track.py <audio file> --public --license "Public domain" --source URL --credit TEXT ...

Any file ffmpeg reads works. The tool cuts --length seconds from --start (default 0 and 40), filters for the pack's
small speaker (high-pass 120 Hz, low-pass 4.5 kHz, a gentle compressor), resamples to mono at 10 kHz, normalizes the
peak to -1 dBFS and encodes IMA ADPCM (4 bits a sample, 5 kB a second).

Where tracks go:
  default   out/halloween-tracks/
  --public  tracks/ in this folder, only with --license "Public domain" or "CC0" and a --source URL
The build includes both folders (tools/tracks2c.py). The board's flash holds about 170 s of tracks in all."""
import argparse, json, os, pathlib, re, subprocess, sys, hashlib, array

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
PRIVATE = pathlib.Path(os.environ.get("TRACKS_PRIVATE", "out/halloween-tracks")).resolve()
PUBLIC = ROOT / "tracks"
RATE = 10000
ALLOWED = ("public domain", "cc0")

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
        118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
        1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
        6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def ima_encode(x):
    """IMA ADPCM, the same arithmetic as halloween/ba_adpcm.c; returns (bytes, decoded samples)."""
    pred, idx, out, dec = 0, 0, bytearray(), []
    nibs = []
    for s in x:
        st = STEP[idx]; diff = s - pred; nib = 0
        if diff < 0: nib = 8; diff = -diff
        if diff >= st: nib |= 4; diff -= st
        if diff >= st >> 1: nib |= 2; diff -= st >> 1
        if diff >= st >> 2: nib |= 1
        # decode exactly as the board does
        d = st >> 3
        if nib & 4: d += st
        if nib & 2: d += st >> 1
        if nib & 1: d += st >> 2
        pred = pred - d if nib & 8 else pred + d
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IDX[nib]))
        nibs.append(nib); dec.append(pred)
    if len(nibs) % 2: nibs.append(0)
    for i in range(0, len(nibs), 2): out.append(nibs[i] | (nibs[i + 1] << 4))
    return bytes(out), dec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src"); ap.add_argument("--name"); ap.add_argument("--title")
    ap.add_argument("--start", type=float, default=0.0); ap.add_argument("--length", type=float, default=40.0)
    ap.add_argument("--public", action="store_true"); ap.add_argument("--license", default="")
    ap.add_argument("--source", default=""); ap.add_argument("--credit", default="")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    src = pathlib.Path(a.src)
    name = a.name or re.sub(r"[^a-z0-9_]", "_", src.stem.lower())[:40]
    if a.public:
        if a.license.strip().lower() not in ALLOWED or not a.source:
            sys.exit("--public needs --license 'Public domain' or 'CC0' and a --source URL")
        dest = PUBLIC
    else:
        dest = PRIVATE
    dest.mkdir(parents=True, exist_ok=True)
    flt = "highpass=f=120,lowpass=f=4500,acompressor=threshold=-24dB:ratio=3:attack=5:release=120:makeup=2"
    raw = subprocess.run(["ffmpeg", "-v", "error", "-ss", str(a.start), "-t", str(a.length), "-i", str(src), "-af", flt,
                          "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"], capture_output=True, check=True).stdout
    x = array.array("h", raw)
    peak = max(1, max(abs(v) for v in x))
    g = 29204 / peak                                       # -1 dBFS
    fade = int(RATE * 0.3)                                 # 0.3 s fades at both ends
    y = []
    for i, v in enumerate(x):
        f = min(1.0, i / fade, (len(x) - 1 - i) / fade)
        y.append(int(max(-32768, min(32767, v * g * f))))
    data, dec = ima_encode(y)
    err = (sum((p - q) ** 2 for p, q in zip(y, dec)) / len(y)) ** 0.5
    (dest / f"{name}.ima").write_bytes(data)
    meta = {"name": name, "title": a.title or src.stem, "rate": RATE, "nsamples": len(y), "bytes": len(data),
            "start_s": a.start, "length_s": round(len(y) / RATE, 2), "credit": a.credit, "license": a.license or "owner's own copy",
            "source": a.source, "source_sha256": hashlib.sha256(src.read_bytes()).hexdigest(), "adpcm_rms_error": round(err, 1),
            "filter": flt + ", peak -1 dBFS, 0.3 s fades"}
    (dest / f"{name}.json").write_text(json.dumps(meta, indent=1) + "\n")
    print(f"{name}: {meta['length_s']} s at {RATE} Hz, {len(data)} bytes, ADPCM error {err:.0f} RMS of 32768, into {dest}")
    if not a.no_build:
        subprocess.run([str(ROOT / "boards/mspm33c321a/build.sh"), "halloween"], check=True)


if __name__ == "__main__":
    main()

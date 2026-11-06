#!/usr/bin/env python3
"""Build the decoy sound bank: real sounds from this Mac resampled to the board's rate (15,873 Hz),
mono int16 .npy, one file per source, with a manifest that puts every source in train or test
(never both), so the held-out score is on sounds the model never heard.

  bank.py OUTDIR

Sources: macOS ToneLibrary ringtones and alert tones (iOS sounds: Alarm, Doorbell, Kettle, Digital...),
/System/Library/Sounds, two music tracks, and speech from macOS `say` in many voices (TV-like talk).
Stays on this Mac (outside the repo): these are Apple's and others' recordings, not ours to publish.
"""
import json
import os
import subprocess
import sys
import zlib

import numpy as np

FS = 32e6 / 7 / 96 / 3
TONES = "/System/Library/PrivateFrameworks/ToneLibrary.framework/Versions/A/Resources"
MUSIC = [os.path.expanduser("~/Music/iTunes/iTunes Media/Music/Taylor Swift/This Love - Single/01 This Love.m4a"),
         os.path.expanduser("~/Music/iTunes/iTunes Media/Music/Robert Munsch/Favourite Stories/05 The Paper Bag Princess.mp3")]
SENTENCES = [
    "Good evening, and welcome to the news at six. Our top story tonight is the storm moving up the coast.",
    "Coming up after the break, we will look at the weekend forecast and what it means for your commute.",
    "The home team scored twice in the final minute to take the lead, and the crowd went wild.",
    "Add two cups of flour, a pinch of salt, and stir until the mixture is smooth.",
    "Don't forget, the meeting has moved to Thursday at ten, so please update your calendar.",
    "Can you grab the milk on your way home? We are almost out, and the kids want cereal.",
    "In other news, the city council voted to approve the new park on the east side.",
    "This product is not available in all areas. Call now, operators are standing by.",
    "Once upon a time, in a land far away, there lived a dragon who loved to read books.",
    "Turn left in five hundred feet, then continue straight for two miles.",
    "Ladies and gentlemen, we are now beginning our descent. Please return your seats to the upright position.",
    "The quarterback drops back, looks downfield, and throws it deep. Touchdown!",
]


def ffm(src, dst_npy):
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", src, "-ac", "1", "-ar", "15873", "-f", "s16le", "-"],
                       capture_output=True)
    if r.returncode or not r.stdout:
        return 0
    x = np.frombuffer(r.stdout, np.int16)
    np.save(dst_npy, x)
    return len(x)


def split_of(name):
    return "test" if zlib.crc32(name.encode()) % 4 == 0 else "train"   # about a quarter held out


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    man = []
    srcs = []
    for root, _, files in os.walk(TONES):
        for f in sorted(files):
            if f.endswith((".m4r", ".caf")):
                srcs.append(("tone", os.path.join(root, f)))
    for f in sorted(os.listdir("/System/Library/Sounds")):
        srcs.append(("tone", os.path.join("/System/Library/Sounds", f)))
    for m in MUSIC:
        if os.path.exists(m):
            srcs.append(("music", m))
    for kind, p in srcs:
        name = kind + "_" + os.path.splitext(os.path.relpath(p, TONES if p.startswith(TONES) else os.path.dirname(p)))[0].replace("/", "_").replace(" ", "_")
        n = ffm(p, os.path.join(out, name + ".npy"))
        if n:
            man.append(dict(name=name, kind=kind, src=p, seconds=round(n / FS, 2), split=split_of(name)))
    voices = [ln.split()[0] for ln in subprocess.run(["say", "-v", "?"], capture_output=True, text=True).stdout.splitlines()
              if "en_" in ln and len(ln.split()[0]) > 1]
    voices = voices[:40]
    for i, v in enumerate(voices):
        aiff = os.path.join(out, "_tmp.aiff")
        text = " ".join(SENTENCES[(i + k) % len(SENTENCES)] for k in range(4))
        if subprocess.run(["say", "-v", v, "-o", aiff, text], capture_output=True).returncode:
            continue
        name = f"speech_{v}"
        n = ffm(aiff, os.path.join(out, name + ".npy"))
        if n:
            man.append(dict(name=name, kind="speech", src=f"say -v {v}", seconds=round(n / FS, 2), split=split_of(name)))
    if os.path.exists(os.path.join(out, "_tmp.aiff")):
        os.remove(os.path.join(out, "_tmp.aiff"))
    json.dump(man, open(os.path.join(out, "manifest.json"), "w"), indent=1)
    tot = {k: round(sum(m["seconds"] for m in man if m["kind"] == k) / 60, 1) for k in ("tone", "music", "speech")}
    print(f"{len(man)} sources, minutes {tot}, test sources {sum(m['split']=='test' for m in man)}")


if __name__ == "__main__":
    main()

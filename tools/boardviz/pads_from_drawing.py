#!/usr/bin/env python3
"""Find the two-pad parts (resistors, capacitors) on a board drawing and write them as boardviz parts.

  pads_from_drawing.py <drawing.png> --crop x0 y0 x1 y1 --scale <px per mm> --out parts.json

The drawing is the board's top-down picture (the one in its user guide). The crop is the board's
pixel extent; the scale is measured from a 2.54 mm header pitch in the same picture. Gold pads are
found by color, paired when two of the same size sit 0.9 to 2.3 mm apart on one axis, and written as
{"type": "chip", "x", "y", "rot", "len", "w"} in millimeters from the board's bottom-left corner.
Headers and large parts are placed by hand in data/boardviz/<board>.json; this only does the decoration.
"""
import argparse, json, math
from collections import deque
import numpy as np
from PIL import Image


def blobs_of(mask):
    H, W = mask.shape
    seen = np.zeros_like(mask)
    ys, xs = np.nonzero(mask)
    out = []
    for y, x in zip(ys, xs):
        if seen[y, x]:
            continue
        q = deque([(y, x)]); seen[y, x] = 1; pts = []
        while q:
            cy, cx = q.popleft(); pts.append((cy, cx))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < H and 0 <= nx < W and mask[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = 1; q.append((ny, nx))
        a = np.array(pts)
        out.append(dict(cy=float(a[:, 0].mean()), cx=float(a[:, 1].mean()), n=len(pts), h=int(np.ptp(a[:, 0]) + 1), w=int(np.ptp(a[:, 1]) + 1)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("drawing"); ap.add_argument("--crop", nargs=4, type=int, required=True)
    ap.add_argument("--scale", type=float, required=True); ap.add_argument("--out", required=True)
    a = ap.parse_args()
    x0, y0, x1, y1 = a.crop
    im = np.array(Image.open(a.drawing).convert("RGB")).astype(int)[y0:y1, x0:x1]
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    gold = (r > 190) & (g > 140) & (g < 215) & (b < 120) & (r - b > 100)
    H = gold.shape[0]; S = a.scale
    blobs = [bb for bb in blobs_of(gold) if bb["n"] >= 6]
    for bb in blobs:
        bb["x"] = bb["cx"] / S; bb["y"] = (H - bb["cy"]) / S; bb["wm"] = bb["w"] / S; bb["hm"] = bb["h"] / S
    small = sorted([i for i, bb in enumerate(blobs) if bb["wm"] <= 1.3 and bb["hm"] <= 1.3], key=lambda i: -blobs[i]["n"])
    used = set(); parts = []
    for i in small:
        if i in used:
            continue
        best = None
        for j in small:
            if j == i or j in used:
                continue
            p, q = blobs[i], blobs[j]
            if abs(p["n"] - q["n"]) > 0.5 * max(p["n"], q["n"]):
                continue
            dx, dy = q["x"] - p["x"], q["y"] - p["y"]; d = math.hypot(dx, dy)
            if 0.9 <= d <= 2.3 and (abs(dx) < 0.2 or abs(dy) < 0.2) and (best is None or d < best[0]):
                best = (d, j)
        if best:
            d, j = best; p, q = blobs[i], blobs[j]
            horiz = abs(q["y"] - p["y"]) < 0.2
            padw = p["wm"] if horiz else p["hm"]; padl = p["hm"] if horiz else p["wm"]
            parts.append(dict(type="chip", x=round((p["x"] + q["x"]) / 2, 2), y=round((p["y"] + q["y"]) / 2, 2), rot=0 if horiz else 90, len=round(d + padw, 2), w=round(padl, 2)))
            used |= {i, j}
    json.dump(parts, open(a.out, "w"), indent=0)
    print(f"{len(parts)} two-pad parts from {len(blobs)} pads; board {gold.shape[1] / S:.1f} x {H / S:.1f} mm")


if __name__ == "__main__":
    main()

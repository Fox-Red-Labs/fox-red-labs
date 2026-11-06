#!/usr/bin/env python3
"""Turn any video into VIDEO.BIN for the BOOSTXL-SHARP128 player: 1-bit 128 x 128 frames.

    python3 video_to_frames.py input.mp4 VIDEO.BIN --fps 15
    python3 video_to_frames.py --synthetic clip.mp4          # make a test clip of moving shapes
    python3 video_to_frames.py input.mp4 VIDEO.BIN --preview 10   # also print frame 10 as text

Needs ffmpeg on the PATH (nothing else: plain Python 3, no extra packages).
File layout: a 512-byte header, then frames of 2,048 bytes.
  header: bytes 0-7 "SH128V1\\0"; 8-9 fps (little endian); 12-15 frame count; 16 width (128); 17 height (128); rest zero
  frame:  128 rows of 16 bytes; bit 0 of byte 0 is the leftmost pixel; 1 = white, 0 = black.
This is the panel's own row layout, so the player sends the bytes as read.
Copy VIDEO.BIN to the root of a FAT32 microSD card (a FAT16 card for the 1 kB host).
"""
import argparse, shutil, subprocess, sys

W = H = 128
FRAME = W * H // 8
HEADER = 512
REV = bytes(int("{:08b}".format(i)[::-1], 2) for i in range(256))   # ffmpeg packs MSB first; the panel is LSB first


def need_ffmpeg():
    if not shutil.which("ffmpeg"):
        sys.exit("ffmpeg not found on PATH (install it, for example: brew install ffmpeg)")


def filter_chain(fit, dither, invert, threshold):
    if fit == "stretch":
        scale = f"scale={W}:{H}"
    elif fit == "crop":
        scale = f"scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H}"
    else:  # letterbox: keep the picture whole, pad with black
        scale = f"scale={W}:{H}:force_original_aspect_ratio=decrease,pad={W}:{H}:(ow-iw)/2:(oh-ih)/2:color=black"
    chain = [scale, "format=gray"]
    if invert:
        chain.append("negate")
    if dither == "none":
        chain.append(f"lut=y='if(gte(val,{threshold}),255,0)'")
    return ",".join(chain)


def convert(src, dst, fps, fit, dither, invert, threshold, limit):
    need_ffmpeg()
    cmd = ["ffmpeg", "-v", "error", "-i", src, "-vf", filter_chain(fit, dither, invert, threshold), "-r", str(fps)]
    if limit:
        cmd += ["-frames:v", str(limit)]
    # monob: 1 bit a pixel, 1 = white, rows padded to bytes; swscale does the dithering
    cmd += ["-pix_fmt", "monob", "-f", "rawvideo"]
    if dither != "none":
        cmd += ["-sws_dither", dither]
    cmd += ["-"]
    p = subprocess.run(cmd, stdout=subprocess.PIPE)
    if p.returncode:
        sys.exit("ffmpeg failed")
    raw = p.stdout
    n = len(raw) // FRAME
    if n == 0:
        sys.exit("no frames produced")
    data = raw[: n * FRAME].translate(REV)
    hdr = bytearray(HEADER)
    hdr[0:8] = b"SH128V1\0"
    hdr[8:10] = fps.to_bytes(2, "little")
    hdr[12:16] = n.to_bytes(4, "little")
    hdr[16] = W
    hdr[17] = H
    with open(dst, "wb") as f:
        f.write(hdr)
        f.write(data)
    return n, data


def show(data, k):
    fr = data[k * FRAME:(k + 1) * FRAME]
    for y in range(0, H, 2):
        row = fr[y * 16:(y + 1) * 16]
        print("".join("#" if not (row[x >> 3] >> (x & 7)) & 1 else "." for x in range(0, W, 1))[::2])


def synthetic(dst, seconds=6, fps=15):
    """Moving shapes: a bouncing box, an orbiting dot, a sweeping line, a rising bar, a frame counter strip."""
    need_ffmpeg()
    vf = (
        "drawbox=x='abs(mod(t*40,200)-100)+14':y='abs(mod(t*27,160)-80)+24':w=24:h=24:color=white:t=fill,"
        "drawbox=x='64+40*sin(t*3)-5':y='64+40*cos(t*3)-5':w=10:h=10:color=white:t=fill,"
        "drawbox=x='t*20':y=2:w=3:h=124:color=gray:t=fill,"
        "drawbox=x=4:y='124-mod(t*30,120)':w=10:h='mod(t*30,120)':color=white:t=fill,"
        "drawbox=x=0:y=0:w=128:h=128:color=white:t=2"
    )
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"color=c=black:s={W}x{H}:r={fps}:d={seconds}",
                    "-vf", vf, "-c:v", "mpeg4", "-q:v", "3", dst], check=True)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", nargs="?")
    ap.add_argument("output", nargs="?", default="VIDEO.BIN")
    ap.add_argument("--fps", type=int, default=15, help="frames per second to keep (default 15)")
    ap.add_argument("--fit", choices=["letterbox", "crop", "stretch"], default="letterbox")
    ap.add_argument("--dither", choices=["none", "bayer", "ed", "a_dither", "x_dither"], default="none",
                    help="none = hard threshold (cartoons, silhouettes); ed = error diffusion; bayer = ordered")
    ap.add_argument("--threshold", type=int, default=128, help="0-255 level for --dither none")
    ap.add_argument("--invert", action="store_true", help="swap black and white (the screen shows 1 as white)")
    ap.add_argument("--limit", type=int, default=0, help="stop after this many frames")
    ap.add_argument("--preview", type=int, default=None, help="print this frame number as text")
    ap.add_argument("--synthetic", metavar="CLIP", help="write a test clip of moving shapes to CLIP and stop")
    a = ap.parse_args()
    if a.synthetic:
        synthetic(a.synthetic, fps=a.fps)
        print("wrote", a.synthetic)
        sys.exit(0)
    if not a.input:
        ap.error("input video required")
    n, data = convert(a.input, a.output, a.fps, a.fit, a.dither, a.invert, a.threshold, a.limit)
    print(f"{a.output}: {n} frames at {a.fps} fps, {HEADER + n * FRAME} bytes")
    if a.preview is not None:
        show(data, min(a.preview, n - 1))

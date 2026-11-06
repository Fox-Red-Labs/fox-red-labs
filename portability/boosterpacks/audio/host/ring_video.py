#!/usr/bin/env python3
"""Render the Halloween light preview: reads <out>/halloween-ring.csv (25 frames a second from host/halloween_preview.c)
and <out>/halloween-preview.wav, writes <out>/halloween-preview.mp4: the eight LEDs where they sit on the Freenove
module (D1 at three o'clock, D2 to D8 clockwise), the LP-AM13E230's two LEDs, the LP-MSPM33C321A's RGB LED, the song title, and the board's own sound.
LED values on the board top out at 64 of 255 (current cap); the picture scales them by 4 so they read on a screen."""
import csv, math, pathlib, re, subprocess, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / "out")
names = re.findall(r'\{ "([^"]+)"', (pathlib.Path(__file__).parent.parent / "halloween" / "ba_setlist.c").read_text())
W = H = 480
FPS = 25
cx, cy, rad, led_r = W // 2, 215, 140, 30

def font(sz):
    for f in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"):
        try: return ImageFont.truetype(f, sz)
        except OSError: pass
    return ImageFont.load_default()
F1, F2 = font(18), font(14)

rows = list(csv.DictReader(open(out / "halloween-ring.csv")))
cmd = ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
       "-i", str(out / "halloween-preview.wav"), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "28", "-preset", "veryfast",
       "-c:a", "aac", "-b:a", "96k", "-shortest", str(out / "halloween-preview.mp4")]
ff = subprocess.Popen(cmd, stdin=subprocess.PIPE)
pos = [(cx + rad * math.cos(math.radians(45 * i)), cy + rad * math.sin(math.radians(45 * i))) for i in range(8)]
for r in rows:
    glow = Image.new("RGB", (W, H), (0, 0, 0))
    g = ImageDraw.Draw(glow)
    for i, (x, y) in enumerate(pos):
        c = tuple(min(255, int(r[f"{k}{i}"]) * 4) for k in "rgb")
        g.ellipse([x - led_r * 1.6, y - led_r * 1.6, x + led_r * 1.6, y + led_r * 1.6], fill=c)
    img = glow.filter(ImageFilter.GaussianBlur(14))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([cx - 190, cy - 190, cx + 190, cy + 190], radius=24, outline=(60, 60, 60), width=2)
    for i, (x, y) in enumerate(pos):
        c = tuple(min(255, int(r[f"{k}{i}"]) * 4) for k in "rgb")
        d.rectangle([x - led_r, y - led_r, x + led_r, y + led_r], fill=tuple(max(v, 28) for v in c), outline=(90, 90, 90))
        d.text((x - 9, y + led_r + 3), f"D{i + 1}", fill=(150, 150, 150), font=F2)
    lc = (int(r["led_r"]), int(r["led_g"]), int(r["led_b"]))
    d.ellipse([370, 424, 404, 458], fill=tuple(max(v, 25) for v in lc), outline=(120, 120, 120))
    d.text((412, 432), "RGB", fill=(170, 170, 170), font=F2)
    for j, (lbl, on, col) in enumerate((("LED4", r["red"], (255, 40, 30)), ("LED5", r["green"], (40, 255, 60)))):
        x0 = 90 + j * 120
        d.ellipse([x0, 430, x0 + 22, 452], fill=col if on == "1" else (40, 40, 40))
        d.text((x0 + 30, 432), lbl, fill=(170, 170, 170), font=F2)
    song = int(r["song"]); title = "between songs" if r["gap"] == "1" else names[song] if song < len(names) else ""
    themes = ["music", "pumpkin", "fire", "lightning", "ghost", "heartbeat", "witch"]; fx = ["clean", "haunted", "graveyard", "mad lab"]
    d.text((16, 56), f"light theme: {themes[int(r['theme'])]}   effects: {fx[int(r['fx'])]}", fill=(170, 150, 200), font=F2)
    d.text((16, 12), title, fill=(230, 200, 150), font=F1)
    d.text((360, 36), f"{float(r['t_s']):6.1f} s", fill=(140, 140, 140), font=F2)
    ff.stdin.write(img.tobytes())
ff.stdin.close(); ff.wait()
print(f"wrote {out / 'halloween-preview.mp4'}: {len(rows)} frames, {len(rows) / FPS:.1f} s" if ff.returncode == 0 else "ffmpeg failed")

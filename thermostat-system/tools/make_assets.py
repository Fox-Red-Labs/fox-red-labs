#!/usr/bin/env python3
"""Regenerate the brand assets for the G3519 thermostat screen.
  Fonts: Barlow (brand/kit/fonts) through lv_font_conv, only the glyphs the screen uses.
  Logo:  the Fox Red Labs horizontal lockup as an 8-bit alpha image (colored at run time), 120 px wide.
Needs node (npx fetches lv_font_conv 1.5.3) and Pillow. Output goes to g3519/fonts/ and g3519/src/."""
import subprocess, pathlib, sys
from PIL import Image
ROOT = pathlib.Path(__file__).resolve().parents[2]
KIT = ROOT / "brand/kit"
OUT = ROOT / "thermostat-system/g3519"
ASCII = "0x20-0x7E"
FONTS = [  # (name, ttf, size, range)
    ("frl_barlow_14", "Barlow-Medium.ttf", 14, ASCII),
    ("frl_barlow_20", "Barlow-SemiBold.ttf", 20, ASCII),
    ("frl_barlow_64", "Barlow-Bold.ttf", 64, "0x20,0x2B,0x2D,0x2E,0x30-0x39,0x43"),   # the setpoint
    ("frl_barlow_48", "Barlow-Bold.ttf", 48, "0x20,0x2B,0x2D,0x2E,0x30-0x39,0x43"),   # space + - . 0-9 C
]
for name, ttf, size, rng in FONTS:
    subprocess.check_call(["npx", "--yes", "lv_font_conv@1.5.3", "--font", str(KIT / "fonts" / ttf), "-r", rng,
        "--size", str(size), "--bpp", "4", "--format", "lvgl", "--lv-include", "lvgl.h", "--no-compress",
        "--lv-font-name", name, "-o", str(OUT / "fonts" / (name + ".c"))])
    f = OUT / "fonts" / (name + ".c")      # keep this machine's paths out of the file header
    f.write_text(f.read_text().replace(str(KIT), "brand/kit").replace(str(OUT), "thermostat-system/g3519"))
W = 120
im = Image.open(KIT / "png/horizontal-red-2000.png").convert("RGBA")
H = round(im.height * W / im.width)
a = im.resize((W, H), Image.LANCZOS).getchannel("A")
data = list(a.tobytes())
lines = ["/* Fox Red Labs horizontal lockup, %dx%d, A8 (alpha only; recolor at run time). Made by tools/make_assets.py */" % (W, H),
         '#include "lvgl.h"', "static const uint8_t frl_logo_map[] = {"]
for i in range(0, len(data), 24):
    lines.append("  " + ",".join("0x%02x" % v for v in data[i:i+24]) + ",")
lines += ["};",
  "const lv_image_dsc_t frl_logo = {",
  "  .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8, .flags = 0, .w = %d, .h = %d, .stride = %d }," % (W, H, W),
  "  .data_size = sizeof(frl_logo_map), .data = frl_logo_map,", "};"]
(OUT / "src/frl_logo.c").write_text("\n".join(lines) + "\n")
print("logo", W, H)

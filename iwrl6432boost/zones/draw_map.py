#!/usr/bin/env python3
"""Draw the zone map (top-down, radar at the bottom) from zones.json into img/zone-map.png. System python3 with Pillow."""
import json, os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__)); FONTS = os.path.join(HERE, "..", "..", "brand", "kit", "fonts")
z = json.load(open(os.path.join(HERE, "zones.json")))
FOX, INK, PAPER, STONE = (180, 36, 24), (22, 19, 18), (244, 239, 230), (107, 98, 91)
def f(n, s): return ImageFont.truetype(os.path.join(FONTS, n), s)
W, H = 1100, 1000; px = 100        # pixels per metre; y runs up the page
img = Image.new("RGB", (W, H), PAPER); d = ImageDraw.Draw(img)
x0 = 400; yb = H - 130
def Y(m): return yb - m * px * 0.95
hw = z["x_half_width_m"]
d.text((40, 24), "Room zones, top-down: radar at the bottom, looking up the room", font=f("Barlow-SemiBold.ttf", 28), fill=INK)
shade = [(226, 232, 214), (214, 224, 232), (232, 222, 214), (230, 214, 218)]
for i, zz in enumerate(z["zones"]):
    y1, y2 = Y(zz["y_max"]), Y(zz["y_min"])
    d.rectangle([x0 - hw * px * 0.6, y1, x0 + hw * px * 0.6, y2], fill=shade[i], outline=STONE, width=2)
    d.text((x0 - hw * px * 0.6 + 14, y1 + 8), zz["name"], font=f("Barlow-Bold.ttf", 30), fill=INK)
    d.text((x0 - hw * px * 0.6 + 14, y1 + 46), f"{zz['y_min']} to {zz['y_max']} m out", font=f("JetBrainsMono.ttf", 20), fill=STONE)
# tape marks every metre
for m in range(0, 8):
    xl = x0 - hw * px * 0.6
    d.line([xl - 14, Y(m), xl, Y(m)], fill=INK, width=2)
    d.text((xl - 70, Y(m) - 11), f"{m} m", font=f("JetBrainsMono.ttf", 18), fill=INK)
# the two chairs on the tape line: nominal 2 m and 4 m, and where the range profile read them in E178 (2.78 and 4.6 m)
for nom, read, name in ((2.0, 2.78, "2 m chair"), (4.0, 4.6, "4 m chair")):
    d.ellipse([x0 + 70 - 12, Y(nom) - 12, x0 + 70 + 12, Y(nom) + 12], outline=INK, width=3)
    d.text((x0 + 95, Y(nom) - 14), f"{name}, tape mark", font=f("Barlow-Medium.ttf", 22), fill=INK)
    d.polygon([(x0 + 70, Y(read) - 10), (x0 + 80, Y(read) + 8), (x0 + 60, Y(read) + 8)], fill=FOX)
    d.text((x0 + 95, Y(read) - 2), f"range profile read {read} m (E178)", font=f("Barlow-Medium.ttf", 20), fill=FOX)
# the radar
d.rectangle([x0 - 40, yb + 8, x0 + 40, yb + 28], fill=FOX)
d.text((x0 + 56, yb + 4), "IWRL6432BOOST on a stand at 1.5 m", font=f("Barlow-SemiBold.ttf", 22), fill=INK)
d.text((40, H - 70), "x is sideways from the radar, y is straight out. Each zone spans the whole width: the left and right split is not reliable.", font=f("Barlow-Regular.ttf", 20), fill=STONE)
d.text((40, H - 40), "Layout from E178 notes, not surveyed with a tape for this drawing. Source: iwrl6432boost/zones/zones.json", font=f("Barlow-Regular.ttf", 20), fill=STONE)
img.save(os.path.join(HERE, "img", "zone-map.png")); print("wrote img/zone-map.png")

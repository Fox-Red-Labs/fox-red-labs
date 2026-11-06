#!/bin/bash
# Re-create third-party/u8g2 from upstream: u8g2 2.37.1, shallow and sparse (only csrc/, LICENSE), pinned.
# Copies the core u8x8 and u8g2 files, one display driver (SSD1306 128x64) and its setup file, and extracts
# two fonts verbatim (u8x8_font_chroma48medium8_r, u8g2_font_6x10_tr) so the 40 MB u8g2_fonts.c and 1.6 MB
# u8x8_fonts.c are not kept. Needs network, git and python3. Usage: fetch.sh [work dir]
set -e
REF=2.37.1; HERE="$(cd "$(dirname "$0")" && pwd)"; W=${1:-$(mktemp -d)}
rm -rf "$W/src"; mkdir -p "$W/src"; cd "$W/src"
git init -q; git remote add origin https://github.com/olikraus/u8g2
git config core.sparseCheckout true; printf 'csrc/\nLICENSE\n!csrc/u8g2_fonts.c\n' > .git/info/sparse-checkout
git fetch -q --depth 1 --filter=blob:none origin tag "$REF"; git checkout -q "$REF"
git cat-file -p "$(git ls-tree HEAD csrc/u8g2_fonts.c | awk '{print $3}')" > u8g2_fonts.c
echo "u8g2 $REF commit $(git rev-parse HEAD)"
cp LICENSE "$HERE/"; mkdir -p "$HERE/csrc" "$HERE/extract"
for f in u8x8.h u8g2.h u8x8_8x8.c u8x8_byte.c u8x8_cad.c u8x8_capture.c u8x8_display.c u8x8_gpio.c u8x8_setup.c u8x8_string.c \
         u8x8_u16toa.c u8x8_u8toa.c u8x8_d_ssd1306_128x64_noname.c u8g2_setup.c u8g2_d_memory.c u8g2_d_setup.c \
         u8g2_buffer.c u8g2_font.c u8g2_hvline.c u8g2_ll_hvline.c u8g2_intersection.c u8g2_box.c u8g2_line.c \
         u8g2_bitmap.c u8g2_cleardisplay.c u8g2_kerning.c u8g2_circle.c; do cp "csrc/$f" "$HERE/csrc/"; done
python3 -I - csrc/u8x8_fonts.c u8g2_fonts.c "$HERE/extract/fonts_extract.c" <<'PY'
import re, sys
u8x8, u8g2, out = sys.argv[1:4]
def grab(path, name):
    t = open(path, encoding="latin-1").read()
    i = t.find("const uint8_t %s[" % name)
    if i < 0: sys.exit("not found: " + name)
    start = t.rfind("/*", 0, i)               # the font's own comment block (name, copyright, glyphs)
    end = re.compile(r'"\s*;').search(t, i).end()  # string data ends with ";  (quotes inside are written \42)
    return t[start:end]
with open(out, "w", encoding="latin-1") as f:
    f.write("/* Extracted verbatim from u8g2 2.37.1 csrc/u8x8_fonts.c and csrc/u8g2_fonts.c by fetch.sh. */\n")
    f.write('#include "u8g2.h"\n\n')
    f.write(grab(u8x8, "u8x8_font_chroma48medium8_r") + "\n\n" + grab(u8g2, "u8g2_font_6x10_tr") + "\n")
PY
( cd "$HERE" && shasum -a 256 LICENSE csrc/* extract/* ) > "$HERE/SHA256SUMS"
echo done

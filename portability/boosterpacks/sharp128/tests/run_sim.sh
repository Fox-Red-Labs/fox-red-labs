#!/bin/bash
# Host simulation of the player path (FatFs full-frame, Petit FatFs line chunks) against a simulated SD card.
# Needs ffmpeg and hdiutil (macOS). Output goes to a temp folder. Usage: tests/run_sim.sh [frames]
set -e
H="$(cd "$(dirname "$0")" && pwd)"; S="$H/.."; R="$S/../../.."
FR="${1:-30}"
O="${TMPDIR:-/tmp}/sharp128-sim"; rm -rf "$O"; mkdir -p "$O/stage"
python3 "$S/player/video_to_frames.py" --synthetic "$O/clip.mp4" --fps 30 >/dev/null
python3 "$S/player/video_to_frames.py" "$O/clip.mp4" "$O/VIDEO.BIN" --fps 30 --limit "$FR" --dither ed
printf 'HELLO FROM THE CARD\n' > "$O/HELLO.TXT"
"$H/make_sd_image.sh" "$O/fat16.img" 16 "$O/VIDEO.BIN" "$O/HELLO.TXT"
"$H/make_sd_image.sh" "$O/fat32.img" 32 "$O/VIDEO.BIN" "$O/HELLO.TXT"
CF="-std=gnu99 -O1 -Wall -Wno-unused-function -Wno-unused-variable -I$S -I$O/stage"
PLAYER() { cc $CF "$@" -Dmain=player_main -c "$S/player/player.c" -o "$O/player.o"; }
echo "== Petit FatFs, FAT16, 8-row chunks (the 1 kB host's path)"
cp "$R/third-party/pff/"{pff.c,pff.h,diskio.h} "$O/stage/"; cp "$S/pffconf.h" "$O/stage/"
PLAYER -DSHARP_WITH_PFF -DSHARP_CHUNK_ROWS=8 -DSHARP_NO_SD_DET
cc $CF -DSHARP_WITH_PFF -DSHARP_NO_SD_DET -o "$O/sim_pff" "$H/host_sd_sim.c" "$O/player.o" "$S/sharp_lcd.c" "$S/sharp_gfx.c" "$S/sharp_sd.c" "$O/stage/pff.c"
"$O/sim_pff" "$O/fat16.img" "$O/VIDEO.BIN"
echo "== Petit FatFs, FAT32, 1-row chunks"
PLAYER -DSHARP_WITH_PFF -DSHARP_CHUNK_ROWS=1 -DSHARP_NO_SD_DET
cc $CF -DSHARP_WITH_PFF -DSHARP_NO_SD_DET -o "$O/sim_pff1" "$H/host_sd_sim.c" "$O/player.o" "$S/sharp_lcd.c" "$S/sharp_gfx.c" "$S/sharp_sd.c" "$O/stage/pff.c"
"$O/sim_pff1" "$O/fat32.img" "$O/VIDEO.BIN"
echo "== FatFs, FAT32, full frames"
rm "$O/stage/"*; cp "$R/third-party/fatfs/"{ff.c,ff.h,diskio.h,ffunicode.c} "$O/stage/"; cp "$S/ffconf.h" "$O/stage/"
PLAYER -DSHARP_WITH_FATFS
cc $CF -DSHARP_WITH_FATFS -o "$O/sim_ff" "$H/host_sd_sim.c" "$O/player.o" "$S/sharp_lcd.c" "$S/sharp_gfx.c" "$S/sharp_sd.c" "$O/stage/ff.c" "$O/stage/ffunicode.c"
"$O/sim_ff" "$O/fat32.img" "$O/VIDEO.BIN"
echo "== hello example with the card (FatFs: folder listing, long names, file read)"
cc -std=gnu99 -O1 -w -I"$S" -I"$O/stage" -DSHARP_WITH_FATFS -DSHARP_PORT_HAS_SD -DBOARD_NAME='"SIM"' -Dmain=hello_main -c "$S/examples/hello/hello.c" -o "$O/hello.o"
cc -std=gnu99 -O1 -w -I"$S" -I"$O/stage" -DSHARP_WITH_FATFS -o "$O/sim_hello" "$H/host_hello_sim.c" "$O/hello.o" "$S/sharp_lcd.c" "$S/sharp_gfx.c" "$S/sharp_sd.c" "$O/stage/ff.c" "$O/stage/ffunicode.c"
"$O/sim_hello" "$O/fat32.img" | tail -1

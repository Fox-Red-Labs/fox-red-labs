#!/bin/bash
# Make the two test clips from synthetic audio (ffmpeg sources, no third-party recording) with lame.
# Output: clips/stereo128.mp3 (44.1 kHz, 128 kbps stereo) and clips/mono64.mp3 (44.1 kHz, 64 kbps mono), 5 s each.
set -e
cd "$(dirname "$0")/../clips"
SRC='aevalsrc=0.25*sin(2*PI*(220+30*sin(2*PI*0.7*t))*t)*(0.6+0.4*sin(2*PI*3*t))+0.12*sin(2*PI*660*t)*exp(-3*mod(t\,0.5))+0.05*sin(2*PI*3520*t)|0.25*sin(2*PI*(330+20*sin(2*PI*0.5*t))*t)*(0.6+0.4*sin(2*PI*2*t))+0.12*sin(2*PI*880*t)*exp(-3*mod(t\,0.5))+0.05*sin(2*PI*5000*t):s=44100:d=5'
ffmpeg -v error -y -f lavfi -i "$SRC" -f lavfi -i "anoisesrc=d=5:c=pink:a=0.04:r=44100:seed=7,pan=stereo|c0=c0|c1=c0" -filter_complex "[0][1]amix=inputs=2:normalize=0" -c:a pcm_s16le ../clips/src_stereo.wav
ffmpeg -v error -y -i src_stereo.wav -ac 1 src_mono.wav
lame --quiet -b 128 -m j src_stereo.wav stereo128.mp3
lame --quiet -b 64 -m m --cbr src_mono.wav mono64.mp3
rm -f src_stereo.wav src_mono.wav
ls -l *.mp3

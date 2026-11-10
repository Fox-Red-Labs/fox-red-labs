#!/bin/bash
# usage: mkraw.sh <outdir>   -> <outdir>/ulisp-raw.bin (256 kB flash image from ulisp.out)
cd "$1"; /Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/hex2000 ulisp.out --ti_txt --memwidth=16 --romwidth=16 -o ulisp.txt >/dev/null 2>&1
python3 - <<'PY'
img=bytearray(b'\xff'*262144); a=0
for l in open('ulisp.txt'):
    l=l.strip()
    if l.startswith('@'): a=int(l[1:],16)-0x100000
    elif l and not l.startswith('q'):
        for b in l.split(): img[a]=int(b,16); a+=1
open('ulisp-raw.bin','wb').write(img)
PY

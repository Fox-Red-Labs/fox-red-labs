#!/bin/bash
# Wrap a Zephyr SRAM image into a boot-ROM image for the AM62L: X.509 certificate + raw binary,
# the format of TRM SPRUJB4A section 5.8, built the way U-Boot's binman (ti-secure-rom, "combined" form,
# tools/binman/btool/openssl.py in ti-u-boot-2025.01) builds tiboot3.bin, but with ONE component (the
# image as the SBL) and no TIFS. Experiment E200. Nothing here touches a board.
#
#   mkphase1.sh [--dry-run] [--bin F] [--elf F] [--load 0x707F0000] [--entry ADDR|auto] [--no-stub]
#                [--key F | --degenerate] [--legacy] [--no-cert] [--two-stage] [--out DIR]
#
# Defaults: BIN=build-sram-all/zephyr/zephyr.bin (override with --bin; on the build host it is under
#   $ZP/), ELF next to it, load 0x707F0000, out ./phase1-out.
# Output: DIR/phase1.bin (send it as tiboot3.bin by XMODEM with am62l-evm/tools/uart_boot.py).
#
# The entry stub. The ROM branches to the load address (TRM 5.5.5.1.3: "the address defined in the Boot
# Info field"); the certificate has no separate entry field. Zephyr's arm64 image starts with its
# vector table and __start sits later (E199: base 0x707F0000, entry 0x707F08BC). So word 0 of the image
# (the "current EL, SP0, synchronous" vector, never used because Zephyr runs on SPx) is replaced with
# `b __start`. --no-stub keeps the image untouched (use it for an image that starts with code).
#
# Certificate forms (--legacy picks the second):
#   ext_boot_info (default): OID 1.3.6.1.4.1.294.1.9, numComp=1, comp1 = SBL: compType 1, bootCore 16 (A53),
#       compOpts 0 (single stage; TRM table 5-55 stage 0xA would make the ROM expect a second stage),
#       destAddr, compSize, sha512. TRM 5.8.5: "Having one component ... is the same as the legacy flow".
#   --legacy: OID ...294.1.1 boot_seq (certType 1, bootCore 16, coreOpts 0, destAddr, imageSize) +
#       ...294.1.2 image_integrity (sha512). Try this if the ROM rejects the first form.
# Keys: default is a fresh random 4096-bit RSA key (binman uses TI's custMpk.pem; any key is expected to
#   pass on a GP or an HS-FS part whose customer key is not fused; unknown until the board says).
#   --degenerate makes the TRM 5.8.6.1.1 key (private exponent 1, 1024 bit), the one TI's SDKs call default.
# --no-cert writes the bare binary (the TRM says the certificate is optional on GP devices).
set -eu
DRY=0; LOAD=0x707F0000; ENTRY=auto; STUB=1; KEY=""; DEG=0; LEGACY=0; NOCERT=0; STAGE=0; OUT=phase1-out
BIN=build-sram-all/zephyr/zephyr.bin; ELF=""
while [ $# -gt 0 ]; do case $1 in
  --dry-run) DRY=1;; --bin) BIN=$2; shift;; --elf) ELF=$2; shift;; --load) LOAD=$2; shift;;
  --entry) ENTRY=$2; shift;; --no-stub) STUB=0;; --key) KEY=$2; shift;; --degenerate) DEG=1;;
  --legacy) LEGACY=1;; --no-cert) NOCERT=1;; --two-stage) STAGE=160;; --out) OUT=$2; shift;;
  *) echo "unknown option $1" >&2; exit 2;; esac; shift; done
[ -n "$ELF" ] || ELF="${BIN%.bin}.elf"
OSSL=${OPENSSL:-openssl}

run() { echo "+ $*"; [ $DRY = 1 ] || "$@"; }
note() { echo "# $*"; }

mkdir -p "$OUT" 2>/dev/null || { [ $DRY = 1 ] || exit 1; }
if [ ! -f "$BIN" ]; then
  [ $DRY = 1 ] || { echo "no $BIN" >&2; exit 1; }
  note "dry run: $BIN not found here, using placeholders for size and hash"
fi

# 1. entry stub and padded payload
if [ -f "$BIN" ]; then
python3 -I - "$BIN" "$ELF" "$LOAD" "$ENTRY" "$STUB" "$OUT/payload.bin" <<'PY'
import sys, struct, os
binf, elf, load, entry, stub, out = sys.argv[1:7]
data = bytearray(open(binf, 'rb').read()); load = int(load, 0)
if entry == 'auto':
    entry = struct.unpack_from('<Q', open(elf, 'rb').read(32), 24)[0] if os.path.exists(elf) else None
    if entry is None: sys.exit("need --elf or --entry for the stub")
else:
    entry = int(entry, 0)
if stub == '1':
    off = (entry - load) // 4
    assert (entry - load) % 4 == 0 and -(1 << 25) <= off < (1 << 25), "entry out of branch range"
    old = struct.unpack_from('<I', data, 0)[0]
    struct.pack_into('<I', data, 0, 0x14000000 | (off & 0x03FFFFFF))
    print(f"# stub: word 0 was {old:#010x}, now `b {entry:#x}` ({0x14000000 | (off & 0x03FFFFFF):#010x})")
else:
    print(f"# no stub: the ROM will run the first word of the image at {load:#x}")
open(out, 'wb').write(data)
print(f"# payload {len(data)} bytes, load {load:#x}, entry {entry:#x}")
PY
SIZE=$(wc -c < "$OUT/payload.bin" | tr -d ' ')
SHA=$(shasum -a 512 "$OUT/payload.bin" | cut -d' ' -f1)
else
  SIZE=53392; SHA=$(printf '0%.0s' $(seq 128))
fi
LOADHEX=$(printf '%08x' $((LOAD)))

if [ $NOCERT = 1 ]; then
  run cp "$OUT/payload.bin" "$OUT/phase1.bin"; note "bare binary, no certificate (GP devices only)"; exit 0
fi

# 2. key
if [ -z "$KEY" ]; then
  KEY="$OUT/phase1-key.pem"
  if [ $DEG = 1 ]; then
    run "$OSSL" genrsa -out "$OUT/rand1024.pem" 1024
    if [ $DRY = 0 ]; then
    "$OSSL" rsa -in "$OUT/rand1024.pem" -text -noout 2>/dev/null | python3 -I -c '
import sys,re
t=sys.stdin.read()
def g(n):
    m=re.search(n+r":\s*\n((?:\s+[0-9a-f:]+\n)+)",t); return "0x"+m.group(1).replace(":","").replace("\n","").replace(" ","")
print("asn1=SEQUENCE:rsa_key\n[rsa_key]\nversion=INTEGER:0\nmodulus=INTEGER:%s\npubExp=INTEGER:1\nprivExp=INTEGER:1\np=INTEGER:%s\nq=INTEGER:%s\ne1=INTEGER:1\ne2=INTEGER:1\ncoeff=INTEGER:%s"%(g("modulus"),g("prime1"),g("prime2"),g("coefficient")))' > "$OUT/degenerateKey.txt"
    fi
    run "$OSSL" asn1parse -genconf "$OUT/degenerateKey.txt" -out "$OUT/degenerateKey.der"
    run "$OSSL" rsa -in "$OUT/degenerateKey.der" -inform der -outform pem -out "$KEY"
  else
    run "$OSSL" genrsa -out "$KEY" 4096
  fi
fi

# 3. certificate config (same fields as binman's openssl.py)
DN="C = US
ST = TX
L = Dallas
O = Texas Instruments Incorporated
OU = Processors
CN = TI Support
emailAddress = support@ti.com"
SHAOID=2.16.840.1.101.3.4.2.3
if [ $LEGACY = 1 ]; then EXT="1.3.6.1.4.1.294.1.1 = ASN1:SEQUENCE:boot_seq
1.3.6.1.4.1.294.1.2 = ASN1:SEQUENCE:image_integrity
1.3.6.1.4.1.294.1.3 = ASN1:SEQUENCE:swrv
1.3.6.1.4.1.294.1.8 = ASN1:SEQUENCE:debug"; BODY="[boot_seq]
certType = INTEGER:1
bootCore = INTEGER:16
bootCoreOpts = INTEGER:$STAGE
destAddr = FORMAT:HEX,OCT:$LOADHEX
imageSize = INTEGER:$SIZE
[image_integrity]
shaType = OID:$SHAOID
shaValue = FORMAT:HEX,OCT:$SHA"
else EXT="1.3.6.1.4.1.294.1.3 = ASN1:SEQUENCE:swrv
1.3.6.1.4.1.294.1.9 = ASN1:SEQUENCE:ext_boot_info
1.3.6.1.4.1.294.1.8 = ASN1:SEQUENCE:debug"; BODY="[ext_boot_info]
extImgSize = INTEGER:$SIZE
numComp = INTEGER:1
sbl = SEQUENCE:sbl
[sbl]
compType = INTEGER:1
bootCore = INTEGER:16
compOpts = INTEGER:$STAGE
destAddr = FORMAT:HEX,OCT:$LOADHEX
compSize = INTEGER:$SIZE
shaType = OID:$SHAOID
shaValue = FORMAT:HEX,OCT:$SHA"
fi
CFG="[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no
dirstring_type = nobmp
[req_distinguished_name]
$DN
[v3_ca]
basicConstraints = CA:true
$EXT
[swrv]
swrv = INTEGER:1
[debug]
debugUID = FORMAT:HEX,OCT:0000000000000000000000000000000000000000000000000000000000000000
debugType = INTEGER:0
coreDbgEn = INTEGER:0
coreDbgSecEn = INTEGER:0
$BODY"
note "certificate config ($OUT/cert.cfg):"
echo "$CFG" | sed 's/^/#   /'
[ $DRY = 1 ] || echo "$CFG" > "$OUT/cert.cfg"

# 4. sign, concatenate (certificate first, image blob immediately after: TRM 5.8.1)
run "$OSSL" req -new -x509 -key "$KEY" -nodes -outform DER -out "$OUT/cert.der" -config "$OUT/cert.cfg" -sha512
echo "+ cat $OUT/cert.der $OUT/payload.bin > $OUT/phase1.bin"
[ $DRY = 1 ] || cat "$OUT/cert.der" "$OUT/payload.bin" > "$OUT/phase1.bin"
note "check: openssl asn1parse -inform der -in $OUT/cert.der | grep -A3 294.1"
[ $DRY = 1 ] || { ls -l "$OUT/phase1.bin"; "$OSSL" asn1parse -inform der -in "$OUT/cert.der" | grep -c "1.3.6.1.4.1.294" | sed 's/^/TI extensions in cert: /'; }

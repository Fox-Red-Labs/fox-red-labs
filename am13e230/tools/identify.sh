#!/bin/bash
# Identify the chip on a new LP-AM13E230 without writing anything to it.
#   am13e230/tools/identify.sh <XDS110 serial>      (serial from: python3 tools/arrival/arrival.py seen)
# 1. Writes am13e230/AM13E23019_<serial>.ccxml: TI's XDS110 connection plus the AM13E23019 device, both from
#    CCStudio's target database (/Applications/ti/ccs2101/ccs/ccs_base/common/targetdb/connections/TIXDS110_Connection.xml
#    and devices/AM13E23019.xml, both present 2026-10-07). Same layout as cc1354p10/CC1354P10.ccxml (another Cortex-M33).
# 2. Reads, read-only, with CCStudio's DSLite: the Arm CPUID (0xE000ED00) and the first 84 bytes of the factory region
#    at 0x60111000: TRACEID, DEVICEID (JTAG ID: version in bits 31-28, part number in bits 27-12), USERID (part,
#    variant, minor and major revision), BSL pin settings, SRAMFLASH (sizes). Layout: AM13E2x SDK 26.01 source/device/am13e230x/include/hw/hw_factoryregion.h; ROM_VERSION is word 19.
# Output: $FRL_OUT if set, else ./identify-am13e230/ in the folder you run it from. If DSLite rejects the ccxml, make one in CCStudio
# (File, New, Target Configuration: connection XDS110, device AM13E23019) and save it under the same name.
set -eu
S=${1:?serial}
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DEV=AM13E23019; FAC=0x60111000; ID=am13e230
C="$REPO/$ID/${DEV}_$S.ccxml"
DSL=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
O="${FRL_OUT:-$PWD/identify-$ID}"; mkdir -p "$O"   # was a folder on one maintainer's Mac; front-door test 2026-10-13
sed -e "s/@SERIAL@/$S/g" -e "s/@DEV@/$DEV/g" "$(dirname "$0")/ccxml.template" > "$C"
"$DSL" memory -c "$C" -r 0xE000ED00,1 -o "$O/cpuid.bin" > "$O/cpuid.log" 2>&1 || { cat "$O/cpuid.log"; exit 1; }
"$DSL" memory -c "$C" -r $FAC,21 -o "$O/factory.bin" > "$O/factory.log" 2>&1 || { cat "$O/factory.log"; exit 1; }
python3 "$(dirname "$0")/decode_ids.py" "$O"

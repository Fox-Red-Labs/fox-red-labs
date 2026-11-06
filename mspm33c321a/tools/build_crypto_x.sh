#!/bin/bash
# Our own crypto timing programs (bench/crypto/<name>.c) on TI's crypto or driverlib example scaffolding, LP-MSPM33C321A.
#   build_crypto_x.sh sha256_sw|sha256_hw|ecdh_p256 [dir]    output <dir>/cx-<name>/ticlang/<example>.out ; RAM struct `xr`
# Same method as build_crypto.sh: SysConfig of the TI example (SYSOSC 32 MHz, as the ECDSA rows), DWT cycle count, results in RAM.
# sha256_sw uses crypto/hash_sha256, ecdh_p256 uses crypto/ec_ecdsa_p256_sign (PKA enabled in SysConfig), sha256_hw uses driverlib/crypto_sha_256.
set -e
N=$1; R="${2:-$PWD/out/speed-run/mspm33c321a}"; O="$R/cx-$N"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
case $N in sha256_sw) E=crypto/hash_sha256; M=hash_sha256;; ecdh_p256) E=crypto/ec_ecdsa_p256_sign; M=ec_ecdsa_p256_sign;; sha256_hw) E=driverlib/crypto_sha_256; M=crypto_sha_256;; *) echo "unknown $N"; exit 2;; esac
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/$E" "$O"
cp "$HERE/bench/crypto/$N.c" "$O/$M.c"
sed -i '' -e 's/--stack_size=256/--stack_size=8192/' -e 's/--heap_size=128/--heap_size=2048/' "$O/ticlang/makefile"
sed -i '' -e 's/--stack_size=4096/--stack_size=8192/' "$O/ticlang/"*.cmd 2>/dev/null || true
[ -n "$XFLAGS" ] && sed -i '' "s|-DCRYPTO_EC_WIN=3|-DCRYPTO_EC_WIN=3 $XFLAGS|" "$O/ticlang/makefile"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "$N built: $O/ticlang/$M.out" || { echo "$N FAILED"; tail -12 "$O/build.log"; exit 1; }

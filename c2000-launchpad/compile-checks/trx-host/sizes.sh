#!/bin/bash
# Prints sizeof for the wire structs on Arm (tiarmclang, the driver's home) and on the C28x (cl2000, C2000Ware's
# 16-bit uint8_t, packed attribute defined away because cl2000 rejects it). Reads the numbers from the compiler's assembly.
HOST=${HOST:-$HOME/zephyr-ti/ws/zephyr/drivers/misc/ti_cc140xp/host}; HERE="$(cd "$(dirname "$0")" && pwd)"; T=$(mktemp -d)
ARM=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; C28=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
"$ARM/bin/tiarmclang" -mcpu=cortex-m33 -S -O0 -I"$HOST" -I"$HERE/stubs" "$HERE/sizes.c" -o "$T/arm.s"
"$C28/bin/cl2000" -v28 -ml -mt --abi=eabi --c99 -D__packed__= --preinclude="$HERE/stubs/c2000ware_u8.h" -I"$HOST" -I"$HERE/stubs" -I"$C28/include" \
  -k --asm_directory="$T" --obj_directory="$T" -c "$HERE/sizes.c" >/dev/null 2>&1
echo "struct arm_bytes c28x_words"
paste <(echo "TRX_TransactionHeader TRX_TransactionFooter TRX_Request_Header TRX_Request_LastStatus TRX_Phy TRX_Request_CommandStore TRX_Request_CommandSubmit TRX_Request_CommandStop TRX_Request_ConfigStore TRX_PowerMode TRX_DeviceConfigData TRX_Request_CommandStore_Transmit_Params" | tr ' ' '\n') \
  <(grep -E '^\s+\.(long|word)\s' "$T/arm.s" | awk '{print $2}') <(grep -E '^\s+\.field\s|^\s+\.long\s|^\s+\.bits\s' "$T/sizes.asm" | awk '{split($2,a,","); print a[1]}' | while read v; do echo $((v)); done) | column -t
rm -rf "$T"

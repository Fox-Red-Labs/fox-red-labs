#!/bin/bash
# Hardware-free prebuild of the AM243x LaunchPad set with the MCU+ SDK makefiles. Touches no probe, port or board.
#   am243x/prebuild.sh        -> logs and summary.txt in <scratch>/runs/arrivals/am243x/prebuild/
set -u
SDK=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33
. "$HOME/tmp/_agent/runs/arrivals/am243x/env.sh"
export CGT_TI_PRU_PATH=$HOME/ti/ti-cgt-pru_2.3.3
OUT=$HOME/tmp/_agent/runs/arrivals/am243x/prebuild; mkdir -p "$OUT"
SUM="$OUT/summary.txt"; printf '%-34s %-28s %-5s %5s  %s\n' example core status sec out_bytes > "$SUM"
EX=$SDK/examples; touch "$OUT/.start"
# example-dir | core_os folder (under am243x-lp/, or firmware/am243x-lp/ for PRU) | note
LIST="
hello_world|r5fss0-0_nortos
drivers/gpio/gpio_led_blink|r5fss0-0_nortos
drivers/i2c/i2c_led_blink|r5fss0-0_nortos
benchmarks/coremark_benchmark|r5fss0-0_nortos
benchmarks/dhrystone_benchmark|r5fss0-0_nortos
benchmarks/benchmark_demo|system_nortos
drivers/boot/sbl_null|r5fss0-0_nortos
drivers/boot/sbl_uart|r5fss0-0_nortos
drivers/boot/sbl_uart_uniflash|r5fss0-0_nortos
drivers/boot/sbl_ospi|r5fss0-0_nortos
drivers/ospi/ospi_flash_diag|r5fss0-0_nortos
drivers/ipc/ipc_notify_echo|system_freertos_nortos
empty|r5fss0-0_nortos
pru_io/empty|r5fss0-0_freertos
pru_io/empty/firmware|icss_g0_pru0_fw
"
echo "$LIST" | while IFS='|' read -r ex core; do
  [ -z "$ex" ] && continue
  case "$ex" in pru_io/empty/firmware) d="$EX/$ex/am243x-lp/$core/ti-pru-cgt";; *) d="$EX/$ex/am243x-lp/$core/ti-arm-clang"; [ -d "$d" ] || d="$EX/$ex/am243x-lp/$core";; esac
  name=$(echo "$ex/$core" | tr '/' '_'); log="$OUT/$name.log"
  if [ ! -d "$d" ]; then printf '%-34s %-28s %-5s %5s  %s\n' "$ex" "$core" MISSING 0 - >> "$SUM"; continue; fi
  s=$(date +%s); (cd "$d" && make -s all PROFILE=release $MAKE_ARGS CGT_TI_PRU_PATH=$CGT_TI_PRU_PATH) > "$log" 2>&1; rc=$?; e=$(date +%s)
  f=$(cd "$d" && ls -t *.release.out *.out *.release.out*  2>/dev/null | head -1); sz=0; [ -n "$f" ] && sz=$(stat -f %z "$d/$f")
  if [ $rc -eq 0 ]; then st=OK; else st=FAIL; sz=0; fi
  printf '%-34s %-28s %-5s %5s  %s\n' "$ex" "$core" $st $((e-s)) "$sz" >> "$SUM"
  [ $rc -ne 0 ] && grep -m1 -i "error" "$log" | sed 's/^/    first error: /' >> "$SUM"
done
{ echo; echo "hs_fs images:"; find "$EX" -path "*am243x-lp*" -name "*.hs_fs" -newer "$OUT/.start" 2>/dev/null | sed "s|$EX/||"; } >> "$SUM"
cat >> "$SUM" <<'N'

Notes
- No enet_lwip: this SDK's examples/networking/lwip is empty (the enet examples ship in the separate Industrial Communications / ENET SDK).
- No motor_control benchmark_demo; benchmarks/benchmark_demo (four R5F cores, system_nortos) is the nearest and builds.
- sbl_* and ospi_flash_diag need SysConfig 1.27.0 (the SDK pin); 1.28.0 and 1.28.1 fail with "Cannot read properties of undefined (reading 'configurables')". All five build with 1.27.0 (2026-11-05).
- ospi_flash_diag ERASES: ospi_flash_diag.c calls OSPI_norFlashErase on the first block, then write/read test. DO NOT RUN.
- sbl_ospi app offset: 0x80000 (bootloader1.appImageOffset, base 0x60080000), in examples/drivers/boot/sbl_ospi/am243x-lp/r5fss0-0_nortos/example.syscfg:42;
  also tools/boot/sbl_prebuilt/am243x-lp/default_sbl_ospi.cfg (bootloader at 0x0, app --flash-offset=0x80000, XIP via flash-mcelf-xip),
  docs/api_guide_am243x/BOOTFLOW_GUIDE.html ("Currently this is 0x80000", assumes sbl_ospi under 512 KB).
N
cat "$SUM"

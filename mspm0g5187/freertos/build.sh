#!/bin/bash
# Build FreeRTOS images for the LP-MSPM0G5187 (MSPM0G5187: Cortex-M0+, 32 MHz here, 128 kB flash, 32 kB SRAM)
# with TI Arm Clang 5.1.1 and the FreeRTOS kernel that ships in MSPM0 SDK 2.11.00.07 (V11.2.0).
#
#   ./build.sh blink         one task blinks LED1 (PA0) every 500 ms
#   ./build.sh bench         task switch, semaphore, notification, queue and interrupt timings
#   ./build.sh bench-lean    the same with configASSERT and the stack overflow check turned off
#   ./build.sh edge-static   the most minimal tasks that link, all memory static
#   ./build.sh edge-heap     the same tasks made with xTaskCreate from the largest heap that links
#   ./build.sh ti-config     the blink with TI's own LP_MSPM0L1306 FreeRTOSConfig.h, unchanged: does it fit?
#   ./build.sh all
#
# Each image goes to build-<name>/: <name>.out, <name>.map, sizes.txt (flash and RAM from the map),
# ram-symbols.txt (every RAM symbol with its size), stack.txt (worst-case stack per task from the
# compiler's stack-usage files) and the objects. The kernel is compiled
# from the SDK's sources (never written to) into build-<name>/ with this folder's FreeRTOSConfig.h.
# Knobs: MIN_STACK (configMINIMAL_STACK_SIZE in words, default 48), MSP_STACK (interrupt and
# startup stack in bytes, default 256 as in TI's FreeRTOS examples).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$(dirname "$0")/../../tools/frl/toolpaths.sh"
FR_CPU_HZ=${FR_CPU_HZ:-32000000}  # 80000000 with FR_GEN=$HERE/../bench/gp/syscfg (SYSPLL, MCLK 80 MHz)
GEN="${FR_GEN:-$HERE/syscfg32}" # SysConfig output of TI gpio_toggle_output on LP_MSPM0G5187: SYSOSC 32 MHz
K="$SDK/kernel/freertos/Source"
MIN_STACK=${MIN_STACK:-48}
MSP_STACK=${MSP_STACK:-256}
TAG=""; [ "$FR_CPU_HZ" != 32000000 ] && TAG="-$((FR_CPU_HZ/1000000))mhz"; [ "$MIN_STACK" != 48 ] && TAG="$TAG-stack$MIN_STACK"; [ "$MSP_STACK" != 256 ] && TAG="$TAG-msp$MSP_STACK"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
INC="-I$HERE -I$GEN -I$K/include -I$K/portable/TI_ARM_CLANG/ARM_CM0 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source"
KSRC=("$K/tasks.c" "$K/list.c" "$K/queue.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/port.c" "$K/portable/TI_ARM_CLANG/ARM_CM0/portasm.c")

# cc <outdir> <defines> <file.c>...
cc() {
    local O="$1" D="$2"; shift 2
    for f in "$@"; do
        (cd "$O" && "$CC/bin/tiarmclang" -O2 $ARCH @"$GEN/device.opt" $INC -gdwarf-3 -Wall -fstack-usage \
            -DFR_MIN_STACK_WORDS=$MIN_STACK -DFR_CPU_HZ=$FR_CPU_HZ $D -c "$f" -o "$(basename "$f" .c).o")
    done
}

# kernel <outdir> <defines>: FreeRTOS kernel and port, compiled once per configuration into
# build-kernel/<configuration>/ and copied in (blink, bench and edge-static share one configuration)
kernel() {
    local O="$1" D="$2" KD
    KD="$HERE/build-kernel/hz${FR_CPU_HZ}stack$MIN_STACK$(echo "$D" | tr -d ' -' | tr '=' '_')"
    if [ ! -f "$KD/done" ] || [ "$HERE/FreeRTOSConfig.h" -nt "$KD/done" ]; then
        rm -rf "$KD"; mkdir -p "$KD"
        cc "$KD" "$D" "${KSRC[@]}"
        touch "$KD/done"
    fi
    cp "$KD"/*.o "$KD"/*.su "$O/"
}

# compile <outdir> <defines> <app.c>: kernel, port, app, shared code, SysConfig output, startup
compile() {
    local O="$1" D="$2" APP="$3"
    # the kernel files do not use the heap size, only whether there is a heap
    kernel "$O" "$(echo "$D" | grep -oE -- '-DFR_(SAFETY|HEAP_BYTES)=[0-9]+' | sed -E 's/HEAP_BYTES=[1-9][0-9]*/HEAP_BYTES=8/' | tr '\n' ' ')"
    cc "$O" "$D" "$HERE/fr_common.c" "$HERE/$APP" "$GEN/ti_msp_dl_config.c" \
        "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g518x_ticlang.c"
}

# link <outdir> <name>: non-zero if it does not fit
link() {
    local O="$1" N="$2"
    sed "s/--stack_size=256/--stack_size=$MSP_STACK/" "$GEN/device_linker.cmd" > "$O/device_linker.cmd"
    (cd "$O" && "$CC/bin/tiarmclang" $ARCH -Wl,-u,_c_int00 *.o -l"$GEN/device.cmd.genlibs" "-L$SDK/source" \
        ./device_linker.cmd -Wl,-m,"$N.map" -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a \
        -o "$N.out" > link.log 2>&1)
}

# sram_left <map>: bytes of SRAM the linker left unused
sram_left() { python3 -c "
import re,sys; m=re.search(r'^\s+SRAM\s+\w+\s+\w+\s+\w+\s+(\w+)', open(sys.argv[1]).read(), re.M); print(int(m.group(1),16))" "$1"; }

# report <outdir> <name>: segment check, sizes, RAM symbols, stack depths
report() {
    local O="$1" N="$2"
    "$CC/bin/tiarmreadelf" -lW "$O/$N.out" | python3 -c '
import sys
bad = []
for l in sys.stdin:
    f = l.split()
    if f[:1] == ["LOAD"]:
        v, p, m = int(f[2], 16), int(f[3], 16), int(f[5], 16)
        for a in (v, p):
            if m and not (a + m <= 0x20000 or (0x20200000 <= a and a + m <= 0x20208000)):
                bad.append(l.strip())
if bad:
    print("SEGMENT OUTSIDE MAIN FLASH OR SRAM:", *bad, sep="\n"); sys.exit(1)
print("segments: all inside main flash 0x0-0x20000 and SRAM 0x20200000-0x20208000")'
    python3 "$HERE/mapsize.py" "$O/$N.map" | tee "$O/sizes.txt"
    "$CC/bin/tiarmnm" -S -n --defined-only "$O/$N.out" | awk '$1 ~ /^2020/ && NF == 4' > "$O/ram-symbols.txt"
    "$CC/bin/tiarmobjdump" -d --no-show-raw-insn "$O/$N.out" > "$O/$N.dis"
    python3 "$HERE/stackdepth.py" "$O" "$O/$N.dis" > "$O/stack.txt"
    rm -f "$O/$N.dis" # only needed for the call graph; rebuild to get it back
    grep -E '^ |^Task|^Inter' "$O/stack.txt" | sed 's/   via.*//'
}

image() {
    local NAME="$1" APP="$2" D="$3" O="$HERE/build-$1$TAG"
    rm -rf "$O"; mkdir -p "$O"
    echo "== $NAME: $APP $D, MIN_STACK=$MIN_STACK words, MSP_STACK=$MSP_STACK bytes"
    compile "$O" "$D" "$APP"
    link "$O" "$NAME" || { cat "$O/link.log"; exit 1; }
    report "$O" "$NAME"
}

# edge-static: the most minimal tasks that link. Links 1 and 2 tasks to get the RAM per task,
# predicts the largest count, then proves it: that count links and one more does not.
edge_static() {
    local O="$HERE/build-edge-static$TAG" N=edge-static
    rm -rf "$O"; mkdir -p "$O"
    echo "== edge-static: MIN_STACK=$MIN_STACK words, MSP_STACK=$MSP_STACK bytes"
    compile "$O" "-DEDGE_TASKS=1" edge.c
    link "$O" $N || { cat "$O/link.log"; exit 1; }
    local f1; f1=$(sram_left "$O/$N.map")
    cc "$O" "-DEDGE_TASKS=2" "$HERE/edge.c"; link "$O" $N || { cat "$O/link.log"; exit 1; }
    local per=$(( f1 - $(sram_left "$O/$N.map") )) n
    n=$(( 1 + f1 / per ))
    echo "RAM per extra task: $per bytes; free with 1 task: $f1 bytes; predicted largest: $n" | tee "$O/search.txt"
    while true; do # walk down until n links
        cc "$O" "-DEDGE_TASKS=$n" "$HERE/edge.c"
        if link "$O" $N; then break; fi
        n=$((n - 1))
    done
    while true; do # walk up until n+1 fails
        cc "$O" "-DEDGE_TASKS=$((n + 1))" "$HERE/edge.c"
        if link "$O" $N; then n=$((n + 1)); else break; fi
    done
    echo "EDGE_TASKS=$((n + 1)) does not link: $(grep -m1 -o 'program will not fit.*' "$O/link.log")" | tee -a "$O/search.txt"
    echo "largest that links: EDGE_TASKS=$n" | tee -a "$O/search.txt"
    cc "$O" "-DEDGE_TASKS=$n" "$HERE/edge.c"; link "$O" $N
    report "$O" $N
}

# edge-heap: the largest heap (multiple of 8 bytes) that links, found the same way
edge_heap() {
    local O="$HERE/build-edge-heap$TAG" N=edge-heap h
    rm -rf "$O"; mkdir -p "$O"
    echo "== edge-heap: MIN_STACK=$MIN_STACK words, MSP_STACK=$MSP_STACK bytes"
    compile "$O" "-DFR_HEAP_BYTES=8" edge.c
    cc "$O" "-DFR_HEAP_BYTES=8" "$K/portable/MemMang/heap_4.c"
    link "$O" $N || { cat "$O/link.log"; exit 1; }
    h=$(( (8 + $(sram_left "$O/$N.map")) / 8 * 8 ))
    while true; do
        cc "$O" "-DFR_HEAP_BYTES=$h" "$K/portable/MemMang/heap_4.c"
        if link "$O" $N; then break; fi
        h=$((h - 8))
    done
    cc "$O" "-DFR_HEAP_BYTES=$((h + 8))" "$K/portable/MemMang/heap_4.c"
    if link "$O" $N; then echo "unexpected: $((h + 8)) links too"; exit 1; fi
    echo "largest heap that links: $h bytes ($((h + 8)) does not: $(grep -m1 -o 'program will not fit.*' "$O/link.log"))" | tee "$O/search.txt"
    compile "$O" "-DFR_HEAP_BYTES=$h" edge.c
    cc "$O" "-DFR_HEAP_BYTES=$h" "$K/portable/MemMang/heap_4.c"
    link "$O" $N || { cat "$O/link.log"; exit 1; }
    report "$O" $N
}

# ti-config: blink.c with TI's kernel/freertos/builds/LP_MSPM0L1306/release/FreeRTOSConfig.h unchanged,
# the kernel files TI's projectspec builds for it (heap_4, timers, TI's static-allocation hooks).
# The link result, pass or fail, is saved in build-ti-config/link.log.
ti_config() {
    local O="$HERE/build-ti-config" N=ti-config TCFG="$SDK/kernel/freertos/builds/LP_MSPM0L1306/release"
    rm -rf "$O"; mkdir -p "$O"
    echo "== ti-config: TI's FreeRTOSConfig.h for LP_MSPM0L1306, unchanged"
    local CFLAGS="-O2 $ARCH @$GEN/device.opt -I$TCFG -I$GEN -I$HERE -I$K/include -I$K/portable/TI_ARM_CLANG/ARM_CM0 -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source/ti/posix/ticlang -I$SDK/source -gdwarf-3 -fstack-usage"
    for f in "${KSRC[@]}" "$K/timers.c" "$K/portable/MemMang/heap_4.c" "$SDK/kernel/freertos/dpl/StaticAllocs_freertos.c" \
        "$HERE/fr_common.c" "$HERE/blink.c" "$GEN/ti_msp_dl_config.c" \
        "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g518x_ticlang.c"; do
        (cd "$O" && "$CC/bin/tiarmclang" $CFLAGS -c "$f" -o "$(basename "$f" .c).o" 2>>compile.log)
    done
    if link "$O" $N; then
        echo "links:"; report "$O" $N
    else
        echo "does not link with TI's config:"; grep -E 'error|fit|SRAM|run placement' "$O/link.log" | head -6
    fi
    # what is in .bss, from the objects (there is no map when the link fails)
    for o in "$O"/*.o; do "$CC/bin/tiarmnm" -S --defined-only "$o" | awk -v o="$(basename "$o")" \
        'NF == 4 && ($3 == "b" || $3 == "B") {print o, $4, $2}'; done | python3 -c "
import sys
rows = [(l.split()[0], l.split()[1], int(l.split()[2], 16)) for l in sys.stdin]
print('zero-filled RAM (.bss) symbols, bytes; total', sum(r[2] for r in rows), '(the linker adds alignment)')
for o, n, z in sorted(rows, key=lambda r: -r[2]): print(f'{z:6} {n:48} {o}')" > "$O/bss-symbols.txt"
    head -12 "$O/bss-symbols.txt"
}

case "$1" in
    blink) image blink blink.c "" ;;
    bench) image bench bench.c "" ;;
    bench-lean) image bench-lean bench.c "-DFR_SAFETY=0" ;;
    edge-static) edge_static ;;
    edge-heap) edge_heap ;;
    ti-config) ti_config ;;
    all) for i in blink bench bench-lean edge-static edge-heap; do "$0" $i; done ;;
    *) echo "usage: $0 blink | bench | bench-lean | edge-static | edge-heap | all"; exit 2 ;;
esac

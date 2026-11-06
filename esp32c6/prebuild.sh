#!/bin/bash
# Build everything for the ESP32-C6-DevKitC-1-N8 that needs no board, before it arrives.
# Images land in out/esp32c6/ (outside the repo).
#
#   esp32c6/prebuild.sh            blink, temperature sensor, CoreMark (ours at -O3/-O2/-Os, Espressif's own),
#                                  the four occupancy models (-O3/-O2/-Os), Thread ot_cli, the ESPHome proxy
#                                  and the minimal ESPHome image
#   esp32c6/prebuild.sh --zigbee   also Espressif's Zigbee on_off_light (needs ESP_ZIGBEE_SDK, a clone)
#   esp32c6/prebuild.sh --sweep    also the sweep's build pass: every ESP-IDF example that lists ESP32-C6 (SWEEP_ONLY=1 for the sweep alone)
#
# Rules:
#   - waits while the 1-minute load average is at or above the core count,
#     so a timed run in another session is not skewed;
#   - refuses to start under 6 GB free disk (one ESP-IDF build folder is 100 to 300 MB; the sweep deletes each
#     build folder after it records the size);
#   - installs nothing. If ESP-IDF or ESPHome is missing it prints the install steps read on 2026-10-07 and exits.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/.." && pwd)
OUT="$PWD/out/esp32c6"
ESPHOME_VENV="${ESPHOME_VENV:-$HOME/.venvs/esphome}"
ZIGBEE=0; SWEEP=0
for a in "$@"; do case $a in --zigbee) ZIGBEE=1 ;; --sweep) SWEEP=1 ;; *) echo "unknown option $a"; exit 64 ;; esac; done

CORES=$(sysctl -n hw.ncpu)
while [ "$(sysctl -n vm.loadavg | awk '{print int($2)}')" -ge "$CORES" ]; do
  echo "$(date +%H:%M:%S) load $(sysctl -n vm.loadavg) is at or above $CORES cores; waiting 30 s"; sleep 30
done

FREE_GB=$(df -g "$HOME" | awk 'NR==2 {print $4}')
if [ "$FREE_GB" -lt 6 ]; then echo "Only $FREE_GB GB free; need 6 GB. Clear space first (mac-setup/README.md)."; exit 3; fi

install_idf() {
cat <<'TXT'
ESP-IDF is not set up (no IDF_PATH or idf.py). Install steps read on 2026-10-07 from
https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/macos-setup.html

  Version:   ESP-IDF v6.1 (stable, released 2026-08-27). Python 3.10 or newer.
  Installer: Espressif Installation Manager (EIM) v0.19.0, eim-cli-macos-x64.zip (9.5 MB) on
             https://github.com/espressif/idf-im-ui/releases (Intel Mac build exists).
  Size:      the v6.1 source archive is 1.95 GB (1,951,682,805 bytes, with .git); the tools EIM always
             installs for the ESP32-C6 are a 449 MB download (riscv32-esp-elf GCC esp-15.2.0_20251204
             398 MB, its GDB 55 MB, OpenOCD 3 MB, ROM ELFs 4 MB, clangd 13 MB) plus a Python environment.
             Expect 3 to 4 GB on disk in all; measure it (du -sh) and put it in mac-setup/README.md.
  Disk rule: 12 GB free before a toolchain install (mac-setup/README.md). This Mac had 10 GB on 2026-10-07.

  1. brew install cmake ninja dfu-util          (cmake is not on this Mac; ninja is)
  2. brew tap espressif/eim && brew install eim
  3. eim install -i v6.1                         (or: eim wizard, pick ESP32-C6 only to skip Xtensa tools)
  4. Open a new shell, source the export script EIM prints, then: idf.py --version
  Then run this script again.
TXT
}

install_esphome() {
cat <<TXT
ESPHome is not installed in $ESPHOME_VENV. Steps read on 2026-10-07 from https://esphome.io/guides/installing_esphome/
and https://pypi.org/project/esphome/:

  Version:  ESPHome 2026.9.1 (PyPI, 2026-09-29); needs Python 3.12 to 3.14 (this Mac: 3.14.7 and 3.12).
  Size:     the wheel is 7.1 MB; it pulls PlatformIO 6.1.19 and esptool 5.3.1, and the first compile downloads
            PlatformIO's own ESP32-C6 toolchain and ESP-IDF framework (size not published; measure with
            du -sh ~/.platformio before and after).

  python3 -m venv $ESPHOME_VENV
  $ESPHOME_VENV/bin/pip install esphome==2026.9.1
  $ESPHOME_VENV/bin/esphome version
TXT
}

mkdir -p "$OUT/logs"; LOG="$OUT/logs/prebuild-$(date +%Y%m%d-%H%M%S).txt"
echo "prebuild start $(date '+%Y-%m-%d %H:%M:%S'), load $(sysctl -n vm.loadavg), free ${FREE_GB} GB" | tee "$LOG"

if [ "${SWEEP_ONLY:-0}" != 1 ]; then
# ---------- ESPHome images (Role A first: the proxy unblocks the BTHome join) ----------
if [ -x "$ESPHOME_VENV/bin/esphome" ]; then
  W="$OUT/esphome"; mkdir -p "$W"; cp "$HERE"/proxy/*.yaml "$W/"
  if [ -f "$HERE/proxy/secrets.yaml" ]; then cp "$HERE/proxy/secrets.yaml" "$W/"
  else   # compile-only stand-ins: a build does not need the real Wi-Fi password; never flash this copy
    printf 'wifi_ssid: "compile-only"\nwifi_password: "compile-only-1"\napi_key: "%s"\nota_password: "compile-only"\n' \
      "$(openssl rand -base64 32)" > "$W/secrets.yaml"
    echo "NOTE: proxy/secrets.yaml missing; compiled with stand-in secrets (do not flash these builds)" | tee -a "$LOG"
  fi
  for y in frl-ble-proxy frl-esphome-minimal; do
    # ESPHome builds on its own ESP-IDF (5.5.5 with 2026.9.1); with ESP-IDF v6.1 exported it picks that tree and
    # fails on removed components. Strip the IDF environment for the ESPHome calls.
    t0=$(date +%s); (cd "$W" && env -u IDF_PATH -u IDF_TOOLS_PATH -u IDF_PYTHON_ENV_PATH -u ESP_IDF_VERSION \
        PATH="$(echo "$PATH" | tr ':' '\n' | grep -v '\.espressif' | paste -sd: -)" \
        "$ESPHOME_VENV/bin/esphome" compile "$y.yaml") > "$OUT/logs/esphome-$y.txt" 2>&1
    echo "esphome $y exit $? in $(( $(date +%s) - t0 )) s" | tee -a "$LOG"
    grep -E "RAM:|Flash:" "$OUT/logs/esphome-$y.txt" | tail -2 | tee -a "$LOG"
  done
else
  install_esphome | tee -a "$LOG"
fi

fi   # end of the SWEEP_ONLY skip (ESPHome)
# ---------- ESP-IDF images ----------
if [ -z "${IDF_PATH:-}" ] || ! command -v idf.py >/dev/null 2>&1; then
  install_idf | tee -a "$LOG"; exit 2
fi
echo "ESP-IDF: $(idf.py --version 2>/dev/null) at $IDF_PATH; GCC $(riscv32-esp-elf-gcc -dumpversion 2>/dev/null)" | tee -a "$LOG"

# build <name> <source dir> [extra cmake args...]: out-of-tree build, sdkconfig kept beside the build
build() {
  local name=$1 src=$2; shift 2
  local b="$OUT/$name"; mkdir -p "$b"
  local t0=$(date +%s)
  idf.py -C "$src" -B "$b/build" -DSDKCONFIG="$b/sdkconfig" "$@" set-target esp32c6 > "$OUT/logs/$name.txt" 2>&1
  # ESP-IDF v6.1 defaults to picolibc; the esp-15.2.0_20251204 toolchain on this Mac has no picolibc multilib for
  # the C6, so float printf faults. Newlib has the right library. Patched after set-target so it
  # applies to the IDF examples too, not only to our projects' sdkconfig.defaults.
  sed -i '' -e 's/^# CONFIG_LIBC_NEWLIB is not set$/CONFIG_LIBC_NEWLIB=y/' -e 's/^CONFIG_LIBC_PICOLIBC=y$/# CONFIG_LIBC_PICOLIBC is not set/' "$b/sdkconfig"
  idf.py -C "$src" -B "$b/build" -DSDKCONFIG="$b/sdkconfig" "$@" build >> "$OUT/logs/$name.txt" 2>&1
  local rc=$? t=$(( $(date +%s) - t0 ))
  if [ $rc -eq 0 ]; then
    idf.py -C "$src" -B "$b/build" -DSDKCONFIG="$b/sdkconfig" "$@" size > "$b/size.txt" 2>&1
    echo "ok    $name ($t s) $(grep -E 'Total image size|Used static DRAM|Used stat D/IRAM' "$b/size.txt" | tr -s ' ' | tr '\n' ' ')" | tee -a "$LOG"
  else
    echo "FAIL  $name ($t s): see logs/$name.txt" | tee -a "$LOG"
  fi
}

if [ "${SWEEP_ONLY:-0}" != 1 ]; then   # SWEEP_ONLY=1 esp32c6/prebuild.sh --sweep: the sweep pass alone
# Blink: Espressif's get-started example. Its sdkconfig.defaults.esp32c6 picks the LED-strip driver and its
# Kconfig default pin is GPIO8, the board's addressable LED (checked in v6.1 on GitHub, 2026-10-07).
# Copied so the changed-blink edit (methodology section 4) never touches the IDF tree.
rm -rf "$OUT/src-blink"; cp -R "$IDF_PATH/examples/get-started/blink" "$OUT/src-blink"
build blink "$OUT/src-blink"
rm -rf "$OUT/src-temp"; cp -R "$IDF_PATH/examples/peripherals/temperature_sensor/temp_sensor" "$OUT/src-temp"
build temp-sensor "$OUT/src-temp"

# CoreMark, unmodified EEMBC files + our core_portme. Variant names: gcc-bare-o3-riscv (and -o2, -os).
build coremark-o3 "$HERE/bench/coremark" -DCM_OPT=-O3
build coremark-o2 "$HERE/bench/coremark" -DCM_OPT=-O2
build coremark-os "$HERE/bench/coremark" -DCM_OPT=-Os
# Espressif's own port (idf-extra-components, -O3 plus jump tables, code in IRAM): variant gcc-vendor-o3-iram.
if [ ! -d "$OUT/src-coremark-vendor" ]; then
  (cd "$OUT" && idf.py create-project-from-example "espressif/coremark:coremark_example" >> "$LOG" 2>&1 \
     && mv coremark_example src-coremark-vendor) || echo "FAIL  fetching espressif/coremark (network?)" | tee -a "$LOG"
fi
[ -d "$OUT/src-coremark-vendor" ] && build coremark-vendor "$OUT/src-coremark-vendor"

# The four occupancy models: variants occ_cnn_*-c-o3, -c-o2-flash, -c-os (names as on the other boards).
build occupancy-o3 "$HERE/bench/occupancy" -DOCC_OPT=-O3
build occupancy-o2 "$HERE/bench/occupancy" -DOCC_OPT=-O2
build occupancy-os "$HERE/bench/occupancy" -DOCC_OPT=-Os

# Thread: ESP-IDF's ot_cli (full Thread device), for the join to the Apple TV's network.
rm -rf "$OUT/src-ot-cli"; cp -R "$IDF_PATH/examples/openthread/ot_cli" "$OUT/src-ot-cli"
build ot-cli "$OUT/src-ot-cli"

if [ $ZIGBEE -eq 1 ]; then
  if [ -d "${ESP_ZIGBEE_SDK:-/nonexistent}/examples/home_automation_devices/on_off_light" ]; then
    rm -rf "$OUT/src-zb-light"; cp -R "$ESP_ZIGBEE_SDK/examples/home_automation_devices/on_off_light" "$OUT/src-zb-light"
    build zigbee-light "$OUT/src-zb-light"
  else
    echo "SKIP  zigbee-light: set ESP_ZIGBEE_SDK to a clone of https://github.com/espressif/esp-zigbee-sdk (it asks for ESP-IDF v5.5.4; try v6.1 first, its examples allow >=5.2)" | tee -a "$LOG"
  fi
fi
fi   # end of the SWEEP_ONLY skip

if [ $SWEEP -eq 1 ]; then
  # Build pass of the sweep: every IDF example whose README lists ESP32-C6. Size recorded, build folder deleted.
  S="$OUT/sweep"; mkdir -p "$S"; touch "$S/results.csv"   # appended, so a stopped sweep resumes
  python3 -I - "$IDF_PATH/examples" > "$S/list.txt" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
for cm in sorted(root.rglob("CMakeLists.txt")):
    d = cm.parent
    if "project(" not in cm.read_text(errors="ignore"):
        continue
    readme = d / "README.md"
    head = readme.read_text(errors="ignore")[:600] if readme.exists() else ""
    m = re.search(r"Supported Targets\s*\|(.*)", head)
    if m and "ESP32-C6" in m.group(1):
        print(d)
PY
  echo "sweep: $(wc -l < "$S/list.txt") examples list ESP32-C6" | tee -a "$LOG"
  while read -r ex; do
    n=$(echo "${ex#$IDF_PATH/examples/}" | tr '/' '_'); b="$S/$n"
    F=$(df -k "$HOME" | awk 'NR==2 {print $4}')
    if [ "$F" -lt 3500000 ]; then echo "sweep: stopped, only $F kB free" | tee -a "$LOG"; break; fi
    grep -q "^$n," "$S/results.csv" 2>/dev/null && continue   # resume: skip examples already recorded
    t0=$(date +%s)
    mkdir -p "$b"
    if idf.py -C "$ex" -B "$b/build" -DSDKCONFIG="$b/sdkconfig" set-target esp32c6 > "$b.log" 2>&1; then
      # same newlib switch as build() above
      sed -i '' -e 's/^# CONFIG_LIBC_NEWLIB is not set$/CONFIG_LIBC_NEWLIB=y/' -e 's/^CONFIG_LIBC_PICOLIBC=y$/# CONFIG_LIBC_PICOLIBC is not set/' "$b/sdkconfig"
      if idf.py -C "$ex" -B "$b/build" -DSDKCONFIG="$b/sdkconfig" build >> "$b.log" 2>&1; then r=built; else r=failed; fi
    else r=failed; fi
    sz=$(ls -l "$b"/build/*.bin 2>/dev/null | awk '{print $5}' | sort -n | tail -1)
    echo "$n,$r,$(( $(date +%s) - t0 )),${sz:-}" >> "$S/results.csv"
    if [ "$r" = built ]; then   # keep only what the run pass flashes: flasher_args.json and the .bin files it names
      mkdir -p "$S/img/$n" && (cd "$b/build" && cp flasher_args.json "$S/img/$n/" && python3 -c "import json;[print(v) for v in json.load(open('flasher_args.json'))['flash_files'].values()]" | while read -r f; do mkdir -p "$S/img/$n/$(dirname "$f")"; cp "$f" "$S/img/$n/$f"; done)
    fi
    rm -rf "${b:?}"   # failed builds too: the log beside it keeps the error
  done < "$S/list.txt"
  echo "sweep: $(grep -c ',built,' "$S/results.csv") built of $(wc -l < "$S/results.csv")" | tee -a "$LOG"
fi

echo "prebuild done $(date '+%Y-%m-%d %H:%M:%S'), free $(df -g "$HOME" | awk 'NR==2 {print $4}') GB" | tee -a "$LOG"

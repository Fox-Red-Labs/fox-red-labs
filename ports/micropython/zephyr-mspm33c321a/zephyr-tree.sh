#!/bin/bash
# Make the Zephyr workspace this port builds against: Zephyr main at 1405adc9a604 (2026-10-20) plus open TI pull requests.
#   zephyr-tree.sh <folder> <uart|i2c|spi>
# uart: UART console + GPIO (pull requests 116768, 118511); i2c: 110526 + 118511; spi: 119347 + 118511.
# The I2C and SPI pull requests each carry their own copy of the UNICOMM driver and conflict with each other: one image each.
# Needs git, a venv with west (VENV, default ~/zephyrproject/.venv) and about 1.5 GB in <folder>.
set -euo pipefail
DEST="${1:?folder}"; KIND="${2:?uart|i2c|spi}"
VENV="${VENV:-$HOME/zephyrproject/.venv}"; [ -e "$VENV/bin/activate" ] || { echo "error: VENV not found at $VENV (a python venv with west)" >&2; exit 1; }
. "$VENV/bin/activate"
BASE=1405adc9a6042353ff791573d913fa6b202580a6
case "$KIND" in
  uart) PR=116768; SHA=9975d063bfebc02256b168315ac8f6c21df330f6;;
  i2c)  PR=110526; SHA=3868a23ad47a797f80d7f25f67e238187be9155a;;
  spi)  PR=119347; SHA=41e2efb5c5d843060d4c688efeb8fc316d429d06;;
  *) echo "error: kind must be uart, i2c or spi" >&2; exit 1;;
esac
GPIO_SHA=947aac340145f993845daf216c193f93c3b4a9a4   # pull request 118511
mkdir -p "$DEST"; cd "$DEST"
git clone --filter=blob:none https://github.com/zephyrproject-rtos/zephyr.git zephyr
cd zephyr
git config user.email you@example.com; git config user.name you
git checkout -q -b mspm33 "$BASE"
git fetch -q origin "pull/$PR/head" pull/118511/head
git merge -q --no-edit "$SHA"
if ! git merge -q --no-edit "$GPIO_SHA"; then
  # The two pull requests edit the same lines of the board file and the SoC dtsi; keep both sides.
  python3 - <<'PY'
import re
p='dts/arm/ti/mspm33c/mspm33c.dtsi'; s=open(p).read()
s=re.sub(r"<<<<<<< HEAD\n(.*?)=======\n.*?>>>>>>> [^\n]*\n", r"\1", s, flags=re.S); open(p,'w').write(s)
p='boards/ti/lp_mspm33c321a/lp_mspm33c321a.dts'; s=open(p).read()
s=re.sub(r"<<<<<<< HEAD\n(#include <zephyr/dt-bindings/pinctrl/mspm0-pinctrl.h>\n)=======\n(.*?)>>>>>>> [^\n]*\n", r"\1\2", s, flags=re.S)
s=re.sub(r"<<<<<<< HEAD\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", lambda m: m.group(1)+"\n"+m.group(2), s, flags=re.S)
open(p,'w').write(s)
PY
  ! grep -rq '^<<<<<<<' dts boards/ti/lp_mspm33c321a || { echo "error: unresolved conflict" >&2; exit 1; }
  git add -A; git commit -q -m "merge gpio"
fi
cd ..
west init -l zephyr
west update cmsis_6 picolibc --narrow -o=--depth=1
echo "done: ZW=$PWD"

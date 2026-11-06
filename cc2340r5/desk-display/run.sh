#!/bin/bash
# One command for the desk display. Usage: ./run.sh            serve the page and push time + weather
#   Needs ~/.venvs/ble with bleak (python3 -m venv ~/.venvs/ble && ~/.venvs/ble/bin/pip install bleak).
#   Set DESK_LAT and DESK_LON for weather. Open http://localhost:8000/ for the browser page (stop the Mac script first: one link at a time).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
case "${1:-clock}" in
  page)  cd "$HERE/web" && python3 -m http.server 8000 ;;
  test)  ~/.venvs/ble/bin/python "$HERE/host/desk.py" test ;;
  clock) ~/.venvs/ble/bin/python "$HERE/host/desk.py" clock ;;
  *) echo "usage: run.sh [clock|test|page]" ;;
esac

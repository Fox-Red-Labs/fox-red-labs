#!/usr/bin/env python3
"""Teach Adafruit Blinka about the AM62L EVM (runs on the board, after `pip install adafruit-blinka gpiod`).
Writes the board and chip modules into the installed package, adds the two import-table entries, and adds
the chip and board names plus device-tree detection (`ti,am62l3`, `ti,am62l3-evm`) to Adafruit-PlatformDetect.
Idempotent. Without the detection edits, `BLINKA_FORCECHIP=AM62L BLINKA_FORCEBOARD=AM62L_EVM` selects the board.
  python3 blinka_am62l.py [--undo-note]"""
import importlib.util, json, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def pkg_dir(name):
    spec = importlib.util.find_spec(name)
    return os.path.dirname(spec.origin)


def edit(path, old, new, marker):
    s = open(path).read()
    if marker in s:
        return 'already'
    if old not in s:
        sys.exit(f'{path}: anchor not found: {old!r}')
    open(path, 'w').write(s.replace(old, new, 1))
    return 'patched'


blinka = pkg_dir('adafruit_blinka')
pd = pkg_dir('adafruit_platformdetect')
src = os.path.dirname(blinka)          # site-packages: board_imports.json lives beside adafruit_blinka

os.makedirs(f'{blinka}/microcontroller/am62l', exist_ok=True)
open(f'{blinka}/microcontroller/am62l/__init__.py', 'a').close()
shutil.copy(f'{HERE}/blinka/pin.py', f'{blinka}/microcontroller/am62l/pin.py')
os.makedirs(f'{blinka}/board/ti', exist_ok=True)
open(f'{blinka}/board/ti/__init__.py', 'a').close()
shutil.copy(f'{HERE}/blinka/am62l_evm.py', f'{blinka}/board/ti/am62l_evm.py')

for fn, key, val in (('board_imports.json', 'AM62L_EVM', 'adafruit_blinka.board.ti.am62l_evm'),
                     ('microcontroller_imports.json', 'AM62L', 'adafruit_blinka.microcontroller.am62l')):
    p = f'{src}/{fn}'
    d = json.load(open(p))
    d[key] = val
    json.dump(d, open(p, 'w'), indent=4)
    print(fn, 'has', key)

print('chips.py', edit(f'{pd}/constants/chips.py', 'AM625X = "AM625X"', 'AM625X = "AM625X"\nAM62L = "AM62L"', 'AM62L ='))
print('boards.py', edit(f'{pd}/constants/boards.py', 'BEAGLE_PLAY = "BEAGLE_PLAY"', 'BEAGLE_PLAY = "BEAGLE_PLAY"\nAM62L_EVM = "AM62L_EVM"', 'AM62L_EVM ='))
print('chip.py', edit(f'{pd}/chip.py',
                      '        if self.detector.check_dt_compatible_value("beagle,am67a-beagley-ai"):',
                      '        if self.detector.check_dt_compatible_value("ti,am62l3"):\n            return chips.AM62L\n        if self.detector.check_dt_compatible_value("beagle,am67a-beagley-ai"):',
                      'chips.AM62L'))
print('board.py', edit(f'{pd}/board.py',
                       '        elif chip_id == chips.AM625X:',
                       '        elif chip_id == chips.AM62L:\n            board_id = boards.AM62L_EVM\n        elif chip_id == chips.AM625X:',
                       'boards.AM62L_EVM'))

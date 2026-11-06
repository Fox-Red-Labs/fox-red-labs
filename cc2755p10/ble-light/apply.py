#!/usr/bin/env python3
"""Turn a copy of TI's basic_ble example into the Bluetooth light.
   python3 apply.py <copy of the basic_ble example folder>
Adds frl_light.c/.h, calls FrlLight_start() after TI's SimpleGatt_start(), renames the device to "FRL Light" and puts the
service's 128-bit UUID in the advertising data. Every edit must match exactly once or the script stops."""
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
UUID = "8a570001-4724-4979-ab46-406d217495e5"
# SysConfig writes the number into the advertising data most significant byte first, the air wants the UUID least significant
# byte first, so the number given here is the UUID with its bytes reversed.
WIRE = bytes.fromhex(UUID.replace("-", ""))[::-1].hex()
root = Path(sys.argv[1])


def edit(path, old, new, count=1):
    t = path.read_text()
    if t.count(old) != count:
        sys.exit(f"{path.name}: expected {count} match of {old!r}, found {t.count(old)}")
    path.write_text(t.replace(old, new))


for f in ("frl_light.c", "frl_light.h"):
    shutil.copy2(HERE / f, root / "app" / f)

main = root / "app" / "app_main.c"
edit(main, "#include <app_main.h>\n", '#include <app_main.h>\n#include "frl_light.h"\n')
edit(main, "    status = SimpleGatt_start();\n    if(status != SUCCESS)\n    {\n        // TODO: Call Error Handler\n    }\n",
     "    status = SimpleGatt_start();\n    if(status != SUCCESS)\n    {\n        // TODO: Call Error Handler\n    }\n"
     "    status = FrlLight_start();\n    if(status != SUCCESS)\n    {\n        // TODO: Call Error Handler\n    }\n")

cfg = root / "freertos" / "basic_ble.syscfg"
edit(cfg, 'ble.deviceName = "Basic BLE project";', 'ble.deviceName = "FRL Light";')
edit(cfg, "ble.advSet1.advData1.GAP_ADTYPE_16BIT_MORE = true;\nble.advSet1.advData1.numOfUUIDs16More = 1;\nble.advSet1.advData1.UUID016More = 65520;\n",
     "ble.advSet1.advData1.GAP_ADTYPE_128BIT_COMPLETE = true;\nble.advSet1.advData1.numOfUUIDs128Complete = 1;\n"
     f'ble.advSet1.advData1.UUID0128Complete = system.utils.bigInt("{WIRE}", 16);\n')
edit(cfg, "// More 16-bit UUIDs available\n", "// Complete 128-bit UUID of the Bluetooth light service\n")

mk = root / "freertos" / "ticlang" / "makefile"
edit(mk, "app_Profiles_app_simple_gatt.obj ", "app_Profiles_app_simple_gatt.obj app_frl_light.obj ")
edit(mk, "app_app_main.obj: ../../app/app_main.c",
     "app_frl_light.obj: ../../app/frl_light.c $(SYSCFG_H_FILES)\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n\n"
     "app_app_main.obj: ../../app/app_main.c")
print("applied to", root)

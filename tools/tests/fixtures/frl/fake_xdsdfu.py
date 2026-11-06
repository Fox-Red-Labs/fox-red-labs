#!/usr/bin/env python3
import os
print("USB Device Firmware Upgrade Utility")
for i, s in enumerate(x for x in os.environ.get("FAKE_PROBES", "").split(",") if x):
    print(f"<<<< Device {i} >>>>\nDevice Name:   XDS110 Embed with CMSIS-DAP\nSerial Num:    {s}")

#!/usr/bin/env python3
"""Decode the CPUID and factory-region words read by identify.sh (MSPM33C321A and AM13E230: same layout for the
first three words, per each SDK's hw_factoryregion.h).  python3 decode_ids.py <identify folder>"""
import struct, sys
o = sys.argv[1]
cpuid = struct.unpack("<I", open(o + "/cpuid.bin", "rb").read()[:4])[0]
f = struct.unpack("<21I", open(o + "/factory.bin", "rb").read()[:84])
dev, user = f[1], f[2]
print(f"CPUID    0x{cpuid:08X}  part 0x{(cpuid >> 4) & 0xFFF:03X} (0xD21 is a Cortex-M33), revision r{(cpuid >> 20) & 0xF}p{cpuid & 0xF}")
print(f"TRACEID  0x{f[0]:08X}  (per-chip, from the wafer)")
print(f"DEVICEID 0x{dev:08X}  version {dev >> 28}, part number 0x{(dev >> 12) & 0xFFFF:04X}, manufacturer 0x{(dev >> 1) & 0x7FF:03X} (TI's JEDEC code is 0x017)")
print(f"USERID   0x{user:08X}  part 0x{user & 0xFFFF:04X}, variant 0x{(user >> 16) & 0xFF:02X}, minor rev {(user >> 24) & 0xF}, major rev {(user >> 28) & 0x7}")
print(f"word 7   0x{f[7]:08X}  (SRAMFLASH on the MSPM33C321A, ROM_SRAMFLASH on the AM13E230: memory sizes)")
print(f"word 19  0x{f[19]:08X}  (ROM_VERSION on the AM13E230 at offset 0x4C)")

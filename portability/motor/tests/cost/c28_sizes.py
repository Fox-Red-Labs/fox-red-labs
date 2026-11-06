#!/usr/bin/env python3
"""Print 'text,data,bss' in bytes for C2000 EABI object files (ofd2000 XML). text and data are section sizes. bss is the sum
of the sizes of the symbols in .bss, because the C28x .bss section is block-aligned and its section size carries padding
(a 148 B pair of objects showed as 204 B). Sections in a COMDAT group (inline library functions the linker drops when
unused) and debug sections are skipped. Usage: c28_sizes.py path/to/ofd2000 file.obj ..."""
import re, subprocess, sys
ofd, files = sys.argv[1], sys.argv[2:]
text = data = bss = 0
for f in files:
    xml = subprocess.run([ofd, "-x", "--obj_display=none,sections,symbols", f], capture_output=True, text=True, check=True).stdout
    nobits = set()
    for m in re.finditer(r"<index>([^<]*)</index>\s*<elf32_shdr>.*?<sh_name_string>([^<]*)</sh_name_string>\s*<sh_type>([^<]*)</sh_type>\s*<sh_flags>(.*?)</sh_flags>.*?<sh_size>([^<]*)</sh_size>", xml, re.S):
        idx, name, ty, fl, sz = m.groups(); n = int(sz, 16)
        flags = set(re.findall(r"SHF_\w+", fl))
        if "SHF_ALLOC" not in flags or "SHF_GROUP" in flags or ty not in ("SHT_PROGBITS", "SHT_NOBITS"):
            continue
        if ty == "SHT_NOBITS": nobits.add(idx)
        elif "SHF_WRITE" in flags: data += n
        else: text += n
    for m in re.finditer(r"<elf32_sym>(.*?)</elf32_sym>", xml, re.S):
        b = m.group(1); sz = re.search(r"<st_size>([^<]*)", b); sh = re.search(r"<st_shndx>([^<]*)", b)
        if sz and sh and sh.group(1) in nobits: bss += int(sz.group(1), 16)
print(f"{text},{data},{bss}")

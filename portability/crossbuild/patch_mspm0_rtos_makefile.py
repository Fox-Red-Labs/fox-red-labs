#!/usr/bin/env python3
"""Make an MSPM0 SDK FreeRTOS example makefile build from a terminal with tiarmclang.
All 48 MSPM0 RTOS makefiles link FreeRTOS with '-i${WORKSPACE_LOC}/...' (old TI linker syntax that
tiarmclang rejects) from a CCStudio workspace. This swaps those two lines for one absolute
'-l$(FREERTOS_LIB)'.
Some also name source files by a path on TI's build server (/home/developer/.cache/bazel/...);
those are pointed back at the SDK. Usage: patch_mspm0_rtos_makefile.py <makefile>"""
import re, sys
p = sys.argv[1]
s = open(p).read()
s = re.sub(r'^[ \t]*-i\$\{WORKSPACE_LOC\}[^\n]*\n', '', s, flags=re.M)
s = re.sub(r'-lfreertos_builds_\w+\.lib', '-l$(FREERTOS_LIB)', s)
s = re.sub(r'/home/developer/\.cache/bazel/\S*?/execroot/__main__/', '$(MSPM0_SDK_INSTALL_DIR)/', s)
open(p, 'w').write(s)

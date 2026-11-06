#!/usr/bin/env python3
"""Patches a copy of TI's lighting AppTask.cpp for the smoke/CO build. Usage: patch_apptask.py AppTask.cpp"""
import sys
p = sys.argv[1]
s = open(p).read()
def sub(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)
sub("#include <app/clusters/on-off-server/on-off-server.h>\n", '#include "SmokeAlarm.h"\n')
sub("    LightMgr().SetCallbacks(ActionInitiated, ActionCompleted);\n",
    "    LightMgr().SetCallbacks(ActionInitiated, ActionCompleted);\n    SmokeAlarmInit();\n    SmokeUartStart();\n")
open(p, "w").write(s)

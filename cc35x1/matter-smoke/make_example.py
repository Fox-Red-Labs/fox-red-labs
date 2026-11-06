#!/usr/bin/env python3
"""Run from the root of TI's matter checkout. Builds examples/smoke-co-alarm-app/ti/cc35xx from TI's lighting example:
copies it, swaps the data model (endpoint 1 = Power Source + Smoke CO Alarm, device type 0x0076, taken from the Matter
project's smoke-co-alarm-app zap), replaces the light code with SmokeCoManager, and gives the console UART an RX ring buffer
(the platform logger only transmits) so the board can read SMOKE/CO lines from the Mac."""
import json, os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = "examples/lighting-app/ti/cc35xx"
DST = "examples/smoke-co-alarm-app/ti/cc35xx"
KEEP = "/build/_out_keep_smoke"
if os.path.exists(DST + "/out"):
    shutil.rmtree(KEEP, ignore_errors=True)
    shutil.move(DST + "/out", KEEP)          # keep ninja's objects so a rerun only rebuilds what changed
if os.path.exists(DST):
    shutil.rmtree(DST)
shutil.copytree(SRC, DST, symlinks=True)
if os.path.exists(KEEP):
    shutil.move(KEEP, DST + "/out")

def rd(p): return open(p).read()
def wr(p, s): open(p, "w").write(s)
def sub(s, old, new, count=1):
    assert old in s, "missing: " + old[:60]
    return s.replace(old, new, count)

# ---- data model: lighting zap with the light endpoint type replaced by the Matter project's smoke-co-alarm one ----------
dm = DST + "/data_model"
os.remove(dm + "/lighting-app.zap"); os.remove(dm + "/lighting-app.matter")
z = json.load(open(SRC + "/data_model/lighting-app.zap"))
sc = json.load(open("examples/smoke-co-alarm-app/smoke-co-alarm-common/smoke-co-alarm-app.zap"))
ep = sc["endpointTypes"][1]
assert [d["code"] for d in ep["deviceTypes"]] == [17, 118]
z["endpointTypes"] = [z["endpointTypes"][0], ep]
z["endpoints"] = [z["endpoints"][0],
    {"endpointTypeName": ep["name"], "endpointTypeIndex": 1, "profileId": 259, "endpointId": 1, "networkId": 0, "parentEndpointIdentifier": None}]
json.dump(z, open(dm + "/smoke-co-alarm-app.zap", "w"), indent=2)
wr(dm + "/BUILD.gn", rd(SRC + "/data_model/BUILD.gn").replace("cc35xx-lighting", "cc35xx-smokeco").replace("lighting-app.zap", "smoke-co-alarm-app.zap"))

# ---- app code ---------------------------------------------------------------------------------------------------------
for f in ("LightingManager.cpp", "LightingManager.h", "ZclCallbacks.cpp"):
    os.remove(DST + "/src/" + f)
for f in ("SmokeCoManager.cpp", "SmokeCoManager.h", "ZclCallbacks.cpp"):
    shutil.copy(HERE + "/src/" + f, DST + "/src/" + f)

h = rd(DST + "/src/AppTask.h")
h = sub(h, '#include "LightingManager.h"', '#include "SmokeCoManager.h"\n#include <platform/CHIPDeviceLayer.h>')
h = sub(h, "    void PostLightActionRequest(int32_t aActor, LightingManager::Action_t aAction);\n", "")
h = sub(h, "    static void ActionInitiated(LightingManager::Action_t aAction, int32_t aActor);\n    static void ActionCompleted(LightingManager::Action_t aAction);\n", "")
wr(DST + "/src/AppTask.h", h)

a = rd(DST + "/src/AppTask.cpp")
a = sub(a, "#include <app/clusters/on-off-server/on-off-server.h>\n", "")
a = sub(a, "    cc35xxLogInit();\n", "    cc35xxLogInit();\n    SmokeCoBootMarker();\n")
a = sub(a, "static LED_Handle sAppRedHandle;", "static LED_Handle sAppRedHandle;\nLED_Handle gThermoLedRed;")
a = sub(a, "    sAppRedHandle = LED_open(CONFIG_LED_0, &ledParams);\n", "    sAppRedHandle = LED_open(CONFIG_LED_0, &ledParams);\n    gThermoLedRed = sAppRedHandle;\n")
a = sub(a, """    ret = LightMgr().Init();

    if (ret != CHIP_NO_ERROR)
    {
        PLAT_LOG("LightMgr().Init() failed");
        while (1)
            ;
    }

    LightMgr().SetCallbacks(ActionInitiated, ActionCompleted);
""", """    if (SmokeCoMgr().Init() != 0)
    {
        PLAT_LOG("SmokeCoMgr().Init() failed, buttons only");
    }
""")
a = sub(a, "#define LIGHTING_APPLICATION_IDENTIFY_ENDPOINT 1", "#define LIGHTING_APPLICATION_IDENTIFY_ENDPOINT 1 /* smoke co alarm endpoint */")
i = a.index("void AppTask::ActionInitiated"); j = a.index("void AppTask::PostEvent")
a = a[:i] + a[j:]
i = a.index("// Action initiated callback\nvoid uiTurnOn"); j = a.index("#if (BUTTON_ENABLE == 1)\nvoid AppTask::ButtonLeftEventHandler")
a = a[:i] + a[j:]
a = sub(a, "void uiTurnOn(void);\nvoid uiTurnedOn(void);\nvoid uiTurnOff(void);\nvoid uiTurnedOff(void);\n", "")
a = sub(a, """    case AppEvent::kEventType_Light: {
        actor = aEvent->LightEvent.Actor;
        LightMgr().IsLightOn() ? LightMgr().InitiateAction(actor, LightingManager::OFF_ACTION)
                               : LightMgr().InitiateAction(actor, LightingManager::ON_ACTION);
        break;
    }
""", "")
a = sub(a, "    int32_t actor;\n\n", "")
a = sub(a, """            actor = AppEvent::kEventType_ButtonLeft;
            LightMgr().InitiateAction(actor, LightingManager::ON_ACTION);""", """            SmokeCoMgr().ToggleSmoke();""")
a = sub(a, """            actor = AppEvent::kEventType_ButtonRight;
            LightMgr().InitiateAction(actor, LightingManager::OFF_ACTION);""", """            SmokeCoMgr().ToggleCO();""")
wr(DST + "/src/AppTask.cpp", a)

# ---- BUILD.gn -------------------------------------------------------------------------------------------------------------
b = rd(DST + "/BUILD.gn")
b = b.replace("lighting-app/ti/cc35xx", "smoke-co-alarm-app/ti/cc35xx").replace("cc35xx-lighting", "cc35xx-smokeco")
b = b.replace("lighting_app", "smokeco_app").replace("lighting-example", "smokeco-example")
b = sub(b, '    "${project_dir}/src/LightingManager.cpp",\n', '    "${project_dir}/src/SmokeCoManager.cpp",\n')
wr(DST + "/BUILD.gn", b)

# ---- console UART: add an RX ring buffer so lines sent while no read is pending are not lost -------------------------------
lg = "src/platform/ti/cc35xx/Logging.cpp"
l = rd(lg)
if "FRL_RXBUF" not in l:
    l = sub(l, "#define LOGGING_UART_BAUD_RATE 115200", "#define LOGGING_UART_BAUD_RATE 115200\nstatic uint8_t FRL_RXBUF[64];")
    l = sub(l, "    uartParams.baudRate = LOGGING_UART_BAUD_RATE;\n", "    uartParams.baudRate = LOGGING_UART_BAUD_RATE;\n    uartParams.rxRingBufPtr  = FRL_RXBUF;\n    uartParams.rxRingBufSize = sizeof(FRL_RXBUF);\n")
    wr(lg, l)
print("example written to", DST)

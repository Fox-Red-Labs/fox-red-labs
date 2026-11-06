#!/usr/bin/env python3
"""Run from the root of TI's matter checkout. Builds examples/thermostat/ti/cc35xx from the lighting example:
copies it, swaps the data model (Thermostat on endpoint 1, Occupancy Sensing on endpoint 2), replaces the light code
with ThermostatManager, and adds the second UART to a copy of the platform SysConfig file."""
import json, os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = "examples/lighting-app/ti/cc35xx"
DST = "examples/thermostat/ti/cc35xx"
KEEP = "/build/_out_keep"
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

# ---- data model: lighting zap with the light endpoint type replaced -------------------------------------------------
dm = DST + "/data_model"
os.remove(dm + "/lighting-app.zap"); os.remove(dm + "/lighting-app.matter")
z = json.load(open(SRC + "/data_model/lighting-app.zap"))
t = json.load(open("examples/thermostat/thermostat-common/thermostat.zap"))
o = json.load(open("examples/chef/devices/rootnode_occupancysensor_iHyVgifZuo.zap"))

th = [e for e in t["endpointTypes"] if e["name"] == "MA-thermostat"][0]
th["clusters"] = [c for c in th["clusters"] if c["name"] != "Thermostat User Interface Configuration"]
drop_attr = {"PresetTypes", "ScheduleTypes", "NumberOfPresets", "ActivePresetHandle", "Presets", "SetpointHoldExpiryTimestamp",
             "MaxThermostatSuggestions", "ThermostatSuggestions", "CurrentThermostatSuggestion",
             "ThermostatSuggestionNotFollowingReason", "MinSetpointDeadBand"}
for c in th["clusters"]:
    if c["name"] == "Thermostat":
        for a in c["attributes"]:
            if a["name"] in drop_attr:
                a["included"] = 0
            if a["name"] == "FeatureMap":
                a["defaultValue"] = "0x03"          # heating + cooling
            if a["name"] == "SystemMode":
                a["defaultValue"] = "0x04"          # heat
            if a["name"] == "ControlSequenceOfOperation":
                a["defaultValue"] = "0x04"          # cooling and heating
        for cmd in c["commands"]:
            cmd["isEnabled"] = 1 if cmd["name"] == "SetpointRaiseLower" else 0
oc = [e for e in o["endpointTypes"] if e["deviceTypeName"] == "MA-occupancysensor"][0]
oc["clusters"] = [c for c in oc["clusters"] if c["name"] in ("Descriptor", "Occupancy Sensing")]
for c in oc["clusters"]:
    if c["name"] == "Occupancy Sensing":
        for a in c["attributes"]:
            if a["name"] in ("HoldTime", "HoldTimeLimits", "PIROccupiedToUnoccupiedDelay", "UltrasonicOccupiedToUnoccupiedDelay",
                             "PhysicalContactOccupiedToUnoccupiedDelay", "PIRUnoccupiedToOccupiedDelay", "PIRUnoccupiedToOccupiedThreshold",
                             "UltrasonicUnoccupiedToOccupiedDelay", "UltrasonicUnoccupiedToOccupiedThreshold",
                             "PhysicalContactUnoccupiedToOccupiedDelay", "PhysicalContactUnoccupiedToOccupiedThreshold"):
                a["included"] = 0
oc["name"] = "MA-occupancysensor"
oc["id"] = 3
oc["deviceTypeProfileId"] = 259
th["id"] = 2
z["endpointTypes"] = [z["endpointTypes"][0], th, oc]
z["endpoints"] = [z["endpoints"][0],
    {"endpointTypeName": "MA-thermostat", "endpointTypeIndex": 1, "profileId": 259, "endpointId": 1, "networkId": 0, "parentEndpointIdentifier": None},
    {"endpointTypeName": "MA-occupancysensor", "endpointTypeIndex": 2, "profileId": 259, "endpointId": 2, "networkId": 0, "parentEndpointIdentifier": None}]
json.dump(z, open(dm + "/thermostat-app.zap", "w"), indent=2)
wr(dm + "/BUILD.gn", rd(SRC + "/data_model/BUILD.gn").replace("cc35xx-lighting", "cc35xx-thermostat").replace("lighting-app.zap", "thermostat-app.zap"))

# ---- app code ---------------------------------------------------------------------------------------------------------
for f in ("LightingManager.cpp", "LightingManager.h", "ZclCallbacks.cpp"):
    os.remove(DST + "/src/" + f)
for f in ("ThermostatManager.cpp", "ThermostatManager.h", "ZclCallbacks.cpp"):
    shutil.copy(HERE + "/src/" + f, DST + "/src/" + f)

h = rd(DST + "/src/AppTask.h")
h = sub(h, '#include "LightingManager.h"', '#include "ThermostatManager.h"\n#include <platform/CHIPDeviceLayer.h>')
h = sub(h, "    void PostLightActionRequest(int32_t aActor, LightingManager::Action_t aAction);\n", "")
h = sub(h, "    static void ActionInitiated(LightingManager::Action_t aAction, int32_t aActor);\n    static void ActionCompleted(LightingManager::Action_t aAction);\n", "")
wr(DST + "/src/AppTask.h", h)

a = rd(DST + "/src/AppTask.cpp")
a = sub(a, "#include <app/clusters/on-off-server/on-off-server.h>\n", "")
a = sub(a, "    cc35xxLogInit();\n", "    cc35xxLogInit();\n    ThermostatBootMarker();\n")
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
""", """    if (ThermoMgr().Init() != 0)
    {
        PLAT_LOG("ThermoMgr().Init() failed, buttons only");
    }
""")
a = sub(a, "#define LIGHTING_APPLICATION_IDENTIFY_ENDPOINT 1", "#define LIGHTING_APPLICATION_IDENTIFY_ENDPOINT 1 /* thermostat endpoint */")
# drop the action callbacks and ui functions
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
            LightMgr().InitiateAction(actor, LightingManager::ON_ACTION);""", """            ThermoMgr().StepSetpoint(50); // +0.5 C""")
a = sub(a, """            actor = AppEvent::kEventType_ButtonRight;
            LightMgr().InitiateAction(actor, LightingManager::OFF_ACTION);""", """            ThermoMgr().StepSetpoint(-50); // -0.5 C""")
# Commissioning window: TI ships 300 s; Matter allows up to 900 s (15 min), which gives a person time to scan.
a = sub(a, "#define OPEN_COMMISSIONING_WINDOW_DURATION 300", "#define OPEN_COMMISSIONING_WINDOW_DURATION 900")
wr(DST + "/src/AppTask.cpp", a)

# ---- BUILD.gn, args.gni, sysconfig ------------------------------------------------------------------------------------
b = rd(DST + "/BUILD.gn")
b = b.replace("lighting-app/ti/cc35xx", "thermostat/ti/cc35xx").replace("cc35xx-lighting", "cc35xx-thermostat")
b = b.replace("lighting_app", "thermostat_app").replace("lighting-example", "thermostat-example")
b = sub(b, '    "${project_dir}/src/LightingManager.cpp",\n', '    "${project_dir}/src/ThermostatManager.cpp",\n')
b = sub(b, 'sources = [ "${chip_root}/examples/platform/ti/sysconfig/chip_cc35xx.syscfg" ]', 'sources = [ "${project_dir}/chip_thermostat.syscfg" ]')
wr(DST + "/BUILD.gn", b)
s = rd("examples/platform/ti/sysconfig/chip_cc35xx.syscfg")
s = sub(s, "const UART21                = UART2.addInstance();", "const UART21                = UART2.addInstance();\nconst UART22                = UART2.addInstance();")
s = sub(s, 'Button1.$name     = "CONFIG_BUTTON_0";', 'UART22.$name = "CONFIG_UART2_1";\nUART22.uart.dmaTxChannel.$assign = "DMA_CH0";\nUART22.uart.dmaRxChannel.$assign = "DMA_CH1";\n\nButton1.$name     = "CONFIG_BUTTON_0";')
s = sub(s, 'Button1.button.$suggestSolution', 'UART22.uart.$suggestSolution       = "UART0";\nUART22.uart.txPin.$suggestSolution = "boosterpack.4";\nUART22.uart.rxPin.$suggestSolution = "boosterpack.3";\nButton1.button.$suggestSolution')
wr(DST + "/chip_thermostat.syscfg", s)
print("example written to", DST)

# ---- optional: TI's Wi-Fi CSI presence pipeline (FRL_CSI=1), its RAM trims (FRL_CSI_TRIM=1) and the heap size (FRL_HEAP) ----
import csi_patch
csi_patch.apply(DST, HERE, rd, wr, sub)

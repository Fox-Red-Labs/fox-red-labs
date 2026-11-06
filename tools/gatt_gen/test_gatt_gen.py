#!/usr/bin/env python3
"""Host checks for gatt_gen.py, no board needed:  python3 tools/gatt_gen/test_gatt_gen.py

1. The UUID bytes the generator writes equal the bytes in the hand-written Blinky header,
   which a Mac read correctly from the board on 2026-10-07.
2. The generated TI table has the hand-written table's attributes in the same order
   (same handles, so a phone that cached the old table sees no change).
3. Bad descriptions are refused with a message, not turned into C.
"""
import json, pathlib, re, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
GEN = HERE / "gatt_gen.py"
HAND = ROOT / "ports/phone-app-demos/blinky-cc1354p10/src/handwritten"
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE))
import gatt_gen  # noqa: E402

fails = 0


def check(ok, what):
    global fails
    print(("ok   " if ok else "FAIL ") + what)
    fails += not ok


def macro_bytes(text, name):
    m = re.search(r"#define %s\s+((?:0x[0-9a-fA-F]{2},?\s*\\?\s*){16})" % name, text)
    return [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(1))] if m else None


svc = gatt_gen.load(HERE / "blinky.json")
hand_h = (HAND / "blinky_service.h").read_text()
for macro, u in [("BLINKY_SERV_UUID128", svc["u"])] + [(f"BLINKY_{c['n']['upper']}_UUID128", c["u"]) for c in svc["characteristics"]]:
    check(macro_bytes(hand_h, macro) == list(gatt_gen.air_bytes(u)), f"{macro} bytes match the hand-written header")

with tempfile.TemporaryDirectory() as t:
    subprocess.run([sys.executable, str(GEN), str(HERE / "blinky.json"), "--target", "ti-ble5stack", "--out", t],
                   check=True, capture_output=True)
    gen_c = (pathlib.Path(t) / "blinky_service.c").read_text()
    gen_h = (pathlib.Path(t) / "blinky_service.h").read_text()
for macro in ("BLINKY_SERV_UUID128", "BLINKY_LED_UUID128", "BLINKY_BUTTON_UUID128"):
    check(macro_bytes(gen_h, macro) == macro_bytes(hand_h, macro), f"generated {macro} equals hand-written")


def table(c_text):
    body = re.search(r"gattAttribute_t \w+AttrTbl\[\] =\s*\{(.*?)\n\};", c_text, re.S).group(1)
    return [(m.group(1), m.group(2).replace(" ", "")) for m in re.finditer(r"\{ \{ (\w+), (\w+) \}, ([^,]+),", body)]


hand_t = table((HAND / "blinky_service.c").read_text())
gen_t = table(gen_c)
norm = lambda t: [(a, re.sub(r"^blinky", "", b)) for a, b in t]  # same type, same UUID array
check(norm(hand_t) == norm(gen_t) and len(gen_t) == 6, f"attribute order matches the hand-written table ({len(gen_t)} attributes)")

# 4. The F3 target (ti-f3) and variable-length values, with the two Nordic services
NORDIC = ROOT / "ports/nordic-apps/services"
nus = gatt_gen.load(NORDIC / "nordic_uart.json")
nus_air = "9e ca dc 24 0e e5 a9 e0 93 f3 a3 b5 01 00 40 6e".split()  # Nordic's NUS UUID as it goes on the air
check([f"{x:02x}" for x in gatt_gen.air_bytes(nus["u"])] == nus_air, "Nordic UART service UUID bytes as on the air")
with tempfile.TemporaryDirectory() as t:
    for f in ("led_button", "nordic_uart"):
        subprocess.run([sys.executable, str(GEN), str(NORDIC / f"{f}.json"), "--target", "ti-f3", "--out", t],
                       check=True, capture_output=True)
    uart_c = (pathlib.Path(t) / "nordic_uart_service.c").read_text()
    lbs_c = (pathlib.Path(t) / "led_button_service.c").read_text()
    uart_h = (pathlib.Path(t) / "nordic_uart_service.h").read_text()
check("ti/ble/stack_util/icall/app/icall_ble_api.h" in uart_c and "MAX_NUM_BLE_CONNS" in uart_c and "linkDBNumConns" not in uart_c,
      "ti-f3 uses the F3 include path and MAX_NUM_BLE_CONNS")
check("MIN(" not in uart_c and "CONST " not in uart_c.replace("const ", ""), "ti-f3 uses no MIN or CONST macro")
check("isVar" in uart_c and "nordicUartRxLen = len" in uart_c, "variable-length write stores the length")
check("isVar" not in lbs_c, "fixed-length service has no variable-length code")
check("0x9e, 0xca, 0xdc, 0x24" in uart_h, "generated Nordic UART header carries the on-air UUID bytes")

bad = [({"service": "X", "uuid": "1234", "characteristics": []}, "not a 128-bit UUID"),
       ({"service": "X", "uuid": str(svc["u"]), "characteristics": [{"name": "A", "uuid": str(svc["u"]), "properties": ["read"], "length": 1}]}, "used twice"),
       ({"service": "X", "uuid": str(svc["u"]), "characteristics": [{"name": "A", "uuid": "5b026510-4088-c297-46d8-be6c736a087a", "properties": ["reed"], "length": 1}]}, "properties must be"),
       ({"service": "X", "uuid": str(svc["u"]), "characteristics": [{"name": "A", "uuid": "5b026510-4088-c297-46d8-be6c736a087a", "properties": ["read"], "length": 600}]}, "1 to 512")]
for doc, expect in bad:
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(doc, f)
    r = subprocess.run([sys.executable, str(GEN), f.name, "--target", "zephyr", "--out", tempfile.gettempdir()],
                       capture_output=True, text=True)
    pathlib.Path(f.name).unlink()
    check(r.returncode != 0 and expect in r.stderr, f"refuses bad input: {expect}")

print("all passed" if not fails else f"{fails} failed")
sys.exit(1 if fails else 0)

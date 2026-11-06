#!/usr/bin/env python3
"""Make dashboard.html (one self-contained file) from dashboard.src.html: Barlow from site/fonts/ as data URIs and the
recorded demo from demo-data.json. Run make_demo.py first if the recorded runs changed.
    python3 -I build.py"""
import base64
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "..", "site", "fonts")
css = ""
for weight, f in ((400, "barlow-regular"), (600, "barlow-semibold"), (700, "barlow-bold")):
    b64 = base64.b64encode(open(os.path.join(FONTS, f + ".woff2"), "rb").read()).decode()
    css += "@font-face{font-family:Barlow;font-weight:%d;font-display:swap;src:url(data:font/woff2;base64,%s) format('woff2')}\n" % (weight, b64)
src = open(os.path.join(HERE, "dashboard.src.html")).read()
data = json.dumps(json.load(open(os.path.join(HERE, "demo-data.json"))), separators=(",", ":"))
out = src.replace("/*FONTS*/", css.rstrip()).replace("/*DEMO*/null", data)
open(os.path.join(HERE, "dashboard.html"), "w").write(out)
print("dashboard.html: %d bytes" % len(out.encode()))

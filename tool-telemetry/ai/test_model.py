#!/usr/bin/env python3
"""Compile the generated C model for this computer and check it gives the same class as the Python trees on every
test pull, with and without current. python3 tool-telemetry/ai/test_model.py"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import train  # noqa: E402

HARNESS = r'''
#include <stdio.h>
#include "tt_model.h"
int main(void) {
    unsigned d, ra, rm, ia, ip, vm; int tc;
    while (scanf("%u %u %u %u %u %u %d", &d, &ra, &rm, &ia, &ip, &vm, &tc) == 7) {
        tt_pull_t p = {0};
        p.dur10 = d; p.ravg = ra; p.rmax = rm; p.iavg = ia; p.ipk = ip; p.vmin = vm; p.tc = (int8_t)tc;
        printf("%u\n", tt_model_classify(&p));
    }
    return 0;
}
'''


def main():
    m = json.load(open(os.path.join(HERE, "out", "model.json")))
    rows = train.simulate(1)
    test = [r for r in rows if r["tool"] in set(m["test_tools"])]
    with tempfile.TemporaryDirectory() as tmp:
        open(os.path.join(tmp, "h.c"), "w").write(HARNESS)
        exe = os.path.join(tmp, "h")
        subprocess.check_call(["cc", "-std=c99", "-Wall", "-Werror", "-I", os.path.join(HERE, "out"), "-I",
                               os.path.join(HERE, "..", "proto"), "-o", exe, os.path.join(tmp, "h.c"),
                               os.path.join(HERE, "out", "tt_model.c")])
        lines, want = [], []
        for r in test:
            x = train.to_fw(r)
            for nocur in (0, 1):
                ia, ip = (0, 0) if nocur else (x["iavg"], x["ipk"])
                lines.append("%d %d %d %d %d %d %d" % (x["dur10"], x["ravg"], x["rmax"], ia, ip, x["vmin"], x["tc"] - 128))
                want.append(train.predict(m["models"]["speed" if nocur else "full"], x))
        got = subprocess.run([exe], input="\n".join(lines) + "\n", capture_output=True, text=True).stdout.split()
    bad = sum(int(g) != w for g, w in zip(got, want))
    assert len(got) == len(want) and bad == 0, "C and Python disagree on %d of %d" % (bad, len(want))
    print("OK: C model matches the Python trees on %d test pulls (with and without current)" % len(want))


if __name__ == "__main__":
    main()

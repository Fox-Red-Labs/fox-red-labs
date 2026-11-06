#!/usr/bin/env python3
"""The generic frame and line, C against Python, on fixed and random cases. python3 portability/fielddata/tests/test_fd.py"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, ROOT)
import fd  # noqa: E402

HARNESS = r'''
#include <stdio.h>
#include <string.h>
#include "fd_frame.h"
static void show(const fd_fields_t *f) { int i; printf("app=%u", f->app); for (i = 0; i < f->n; i++) printf(" %u:%ld", f->id[i], (long)f->v[i]); printf("\n"); }
int main(void) {
    char buf[600];
    while (fgets(buf, sizeof buf, stdin)) {
        size_t n = strlen(buf); while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
        if (buf[0] == 'L') { fd_fields_t f; int32_t s; int r = fd_parse_line(buf + 2, n - 2, &f, &s);
            if (r) printf("err %d\n", r); else { printf("n=%ld ", (long)s); show(&f); } }
        else if (buf[0] == 'B') { /* B <seq> <app> id:v ... */
            fd_fields_t f; unsigned seq, app; char *p; uint8_t out[300]; size_t k, i;
            memset(&f, 0, sizeof f); sscanf(buf + 2, "%u %u", &seq, &app); f.app = (uint8_t)app;
            p = strchr(buf + 2, ' '); p = p ? strchr(p + 1, ' ') : NULL;
            while (p && *p) { unsigned id; long v; if (sscanf(p + 1, "%u:%ld", &id, &v) == 2 && f.n < FD_MAX_FIELDS) { f.id[f.n] = (uint8_t)id; f.v[f.n] = (int32_t)v; f.n++; } p = strchr(p + 1, ' '); }
            k = fd_build_ad(out, sizeof out, (uint8_t)seq, &f);
            for (i = 0; i < k; i++) printf("%02x", out[i]); printf("\n"); }
        else if (buf[0] == 'D') { uint8_t m[300]; size_t ml = 0, i; fd_fields_t f; uint8_t seq;
            for (i = 2; i + 1 < n && ml < sizeof m; i += 2) { unsigned x; sscanf(buf + i, "%2x", &x); m[ml++] = (uint8_t)x; }
            if (fd_decode_msd(m, ml, &f, &seq)) printf("none\n"); else { printf("seq=%u ", seq); show(&f); } }
    }
    return 0;
}
'''


def py_line(line):
    try:
        app, n, f = fd.parse_line(line)
    except ValueError as e:
        return "err %d" % {"framing": -1, "checksum": -2, "not fd": -3}[str(e)]
    return ("n=%d app=%d" % (n, app)) + "".join(" %d:%d" % (k, v) for k, v in f.items())


def main():
    r = random.Random(17)
    cmds, wants = [], []
    for _ in range(3000):
        app = r.choice([0, 1, 2, 7, 255, 256, 300])
        fields = {r.randint(-2, 300): r.choice([0, 1, -1, 127, -128, 128, -129, 32767, -32768, 32768, 2**31 - 1, -2**31,
                                                r.randint(-2**31, 2**31 - 1)]) for _ in range(r.randint(0, 20))}
        body = "FD,n=%d,app=%d" % (r.randint(0, 70000), app) + "".join(",%d=%d" % (k, v) for k, v in fields.items())
        if r.random() < 0.1:
            body += ",x=1,5=abc"
        line = "$%s*%s" % (body, fd.checksum(body))
        if r.random() < 0.05:
            line = line[:-2] + "00"
        if r.random() < 0.03:
            line = line.replace("$FD", "$FX")
            line = line[:-2] + fd.checksum(line[1:-3])
        cmds.append("L " + line)
        wants.append(py_line(line))
    for _ in range(2000):
        app, seq = r.randint(1, 255), r.randint(0, 255)
        fields = {}
        for _k in range(r.randint(0, 16)):
            fields[r.randint(1, 255)] = r.choice([5, -5, 300, -300, 70000, -70000, 2**31 - 1, -2**31])
        h = fd.build_ad(app, seq, fields).hex()
        cmds.append("B %d %d " % (seq, app) + " ".join("%d:%d" % kv for kv in fields.items()))
        wants.append(h)
        d = fd.decode_msd(bytes.fromhex(h)[2:])
        cmds.append("D " + h[4:])
        wants.append("none" if d is None else ("seq=%d app=%d" % (d[1], d[0])) + "".join(" %d:%d" % kv for kv in d[2].items()))
        assert d is not None and d[2] == fields and d[0] == app
    for _ in range(1000):
        junk = bytes(r.randint(0, 255) for _ in range(r.randint(0, 40)))
        if r.random() < 0.5:
            junk = b"\xff\xffFR\x21" + junk
        cmds.append("D " + (junk.hex() or "00"))
        d = fd.decode_msd(junk or b"\x00")
        wants.append("none" if d is None else ("seq=%d app=%d" % (d[1], d[0])) + "".join(" %d:%d" % kv for kv in d[2].items()))
    with tempfile.TemporaryDirectory() as tmp:
        open(os.path.join(tmp, "h.c"), "w").write(HARNESS)
        exe = os.path.join(tmp, "h")
        subprocess.check_call(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", os.path.join(ROOT, "c"), "-o", exe,
                               os.path.join(tmp, "h.c"), os.path.join(ROOT, "c", "fd_frame.c")])
        got = subprocess.run([exe], input="\n".join(cmds) + "\n", capture_output=True, text=True).stdout.splitlines()
    assert len(got) == len(wants), (len(got), len(wants))
    for c, g, w in zip(cmds, got, wants):
        assert g == w, "mismatch on %r\n C  %s\n py %s" % (c[:120], g, w)
    for a in fd.load_apps().values():
        ids = [f["id"] for f in a["fields"]]
        assert len(ids) == len(set(ids)) and all(0 < i < 256 for i in ids), a["name"]
    print("OK: %d checks, C and Python agree; %d app schemas valid" % (len(cmds), len(fd.load_apps())))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Latency from the Mac: 100 blocked and 100 allowed names through the sinkhole, and the same allowed names straight to the upstream.
  python3 measure.py <board ip> <upstream ip> <hosts file> [--n 100]
Uses dig only against the two addresses given (never a system resolver setting). Blocked names are the first N of a shuffled
list from the hosts file; allowed names are N distinct names that are not in it (the first pass is a cache miss at the upstream, so
each allowed name is queried twice and both passes are reported). Output: JSON lines and a summary table."""
import json, random, re, statistics, subprocess, sys

def dig(server, name):
    r = subprocess.run(["dig", f"@{server}", name, "A", "+tries=1", "+time=3", "+noall", "+answer", "+stats"],
                       capture_output=True, text=True)
    m = re.search(r"Query time: (\d+) msec", r.stdout)
    ans = re.findall(r"\sIN\s+A\s+(\S+)", r.stdout)
    return (int(m.group(1)) if m else None), ans

def pct(v, p):
    v = sorted(v); return v[min(len(v) - 1, int(round(p / 100 * len(v) + 0.5)) - 1)]

def main():
    board, up, hosts = sys.argv[1:4]
    n = int(sys.argv[sys.argv.index("--n") + 1]) if "--n" in sys.argv else 100
    rnd = random.Random(7)
    listed = sorted({l.split()[1].lower() for l in open(hosts) if l.startswith("0.0.0.0 ") and len(l.split()) > 1 and "." in l.split()[1]})
    blocked = rnd.sample(listed, n)
    allowed = [f"measure-{i}-{rnd.randrange(10**6)}.example.org" for i in range(n)]   # NXDOMAIN at the upstream: a real miss each
    allowed_real = ["www.wikipedia.org", "www.python.org", "github.com", "www.apple.com", "www.debian.org", "www.mozilla.org",
                    "www.kernel.org", "www.gnu.org", "www.iana.org", "www.rfc-editor.org"]
    out = {}
    for label, server, names in [("blocked_board", board, blocked), ("allowed_board_1st", board, allowed), ("allowed_board_2nd", board, allowed),
                                 ("allowed_up_1st", up, [a.replace("measure-", "measureb-") for a in allowed]),
                                 ("allowed_up_2nd", up, [a.replace("measure-", "measureb-") for a in allowed])]:
        t, bad = [], 0
        for nm in names:
            ms, ans = dig(server, nm)
            if ms is None: bad += 1; continue
            if label.startswith("blocked") and ans != ["0.0.0.0"]: bad += 1
            t.append(ms)
        out[label] = {"n": len(t), "bad": bad, "median_ms": statistics.median(t) if t else None, "p95_ms": pct(t, 95) if t else None}
        print(json.dumps({label: out[label]}))
    t, ok = [], 0
    for nm in allowed_real:
        a = dig(board, nm); b = dig(up, nm)
        ok += (a[1] == b[1] or (a[1] and b[1])) and bool(a[1]); t.append((nm, a[1], b[1]))
    print(json.dumps({"real_names_match": ok, "of": len(t), "rows": t[:3]}))

main()

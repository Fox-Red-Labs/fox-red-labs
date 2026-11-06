#!/usr/bin/env python3
"""Analyse the microphone captures the BOOSTXL-AUDIO app prints ("CAP", "S" lines, "STATS", "END").

  python3 tools/mic_analyze.py console.log [--vref 3.3] [--label name]

For every capture: mean and RMS in ADC codes and in millivolts (12-bit, reference --vref volts), the dominant frequency (peak of
a Hann-windowed DFT with parabolic interpolation, 1 Hz to Nyquist, DC excluded) and its level, and the share of the signal energy
within 50 Hz of that peak. Pure Python, no numpy. Prints one line per capture; --json prints a list of dicts."""
import cmath, json, math, re, sys


def captures(text):
    out, cur = [], None
    for ln in text.splitlines():
        ln = ln.strip()
        if ln.startswith("CAP "):
            m = dict(re.findall(r"(\w+)=(\d+)", ln)); cur = {"rate": int(m["rate"]), "samples": []}
        elif ln.startswith("S ") and cur is not None:
            h = ln[2:].strip()
            cur["samples"] += [int(h[i:i + 4], 16) for i in range(0, len(h) - 3, 4)]
        elif ln.startswith("END") and cur is not None:
            out.append(cur); cur = None
    return out


def analyse(c, vref=3.3):
    x, fs = c["samples"], c["rate"]
    n = len(x)
    mean = sum(x) / n
    d = [v - mean for v in x]
    rms = math.sqrt(sum(v * v for v in d) / n)
    w = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    dw = [d[i] * w[i] for i in range(n)]
    half = n // 2
    mag = []
    for k in range(half + 1):
        a = -2j * math.pi * k / n
        s = sum(dw[i] * cmath.exp(a * i) for i in range(n)) if k else 0
        mag.append(abs(s))
    pk = max(range(2, half), key=lambda k: mag[k])
    a, b, g = mag[pk - 1], mag[pk], mag[pk + 1]
    off = 0.5 * (a - g) / (a - 2 * b + g) if (a - 2 * b + g) else 0.0
    f = (pk + off) * fs / n
    band = max(1, int(50 * n / fs))
    e_all = sum(m * m for m in mag[2:]); e_pk = sum(m * m for m in mag[max(2, pk - band):pk + band + 1])
    mv = vref * 1000 / 4096
    return {"n": n, "rate": fs, "mean_code": round(mean, 1), "rms_code": round(rms, 2), "rms_mV": round(rms * mv, 2),
            "min": min(x), "max": max(x), "peak_hz": round(f, 1), "peak_share": round(e_pk / e_all, 3) if e_all else 0}


if __name__ == "__main__":
    a = sys.argv[1:]
    vref = float(a[a.index("--vref") + 1]) if "--vref" in a else 3.3
    caps = captures(open(a[0]).read())
    res = [analyse(c, vref) for c in caps]
    if "--json" in a:
        print(json.dumps(res, indent=1))
    else:
        for i, r in enumerate(res):
            print(f"capture {i}: mean {r['mean_code']} codes, RMS {r['rms_code']} codes ({r['rms_mV']} mV), p-p {r['min']} to {r['max']}, "
                  f"dominant {r['peak_hz']} Hz, {int(r['peak_share'] * 100)} percent of the energy within 50 Hz of it")

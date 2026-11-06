"""A simple, honest stand-in for what a handheld brushless motor load sees during one run.

Four load classes, the labels the model learns:
  0 light   free run, small load: short, near no-load speed, low current
  1 normal  medium load: speed sags a little, moderate current
  2 heavy   large load: long, speed sags a lot, high current, the pack voltage dips, the tool warms
  3 jam     the load binds: current spikes, speed collapses, the pull ends early (the tool trips)

The numbers are made up but shaped like a 20 V brushless handheld unit (no-load about 3,400 rpm here), and the classes overlap
on purpose so a model cannot be perfect. Replace this with real pulls from the AM13E230 rig as they arrive; the
feature names are the same.
"""
import random

CLASSES = ("light", "normal", "heavy", "jam")
FEATURES = ("dur_ms", "rpm_avg", "rpm_max", "i_avg_ma", "i_peak_ma", "v_min_mv", "temp_c")
NO_LOAD_RPM = 3400


def pull(rng: random.Random, cls: int, pack_mv: int = 20000, temp_c: float = 25.0):
    """One pull of class cls. Returns (features dict, new tool temperature)."""
    g = rng.gauss
    if cls == 0:
        dur = rng.uniform(250, 1600)
        load = max(0.0, g(0.08, 0.06))
    elif cls == 1:
        dur = rng.uniform(800, 4500)
        load = min(0.62, max(0.12, g(0.30, 0.09)))
    elif cls == 2:
        dur = rng.uniform(2000, 9000)
        load = min(0.95, max(0.40, g(0.62, 0.11)))
    else:
        dur = rng.uniform(150, 2200)
        load = min(0.9, max(0.15, g(0.40, 0.15)))         # what it was doing before it bound
    rpm_avg = NO_LOAD_RPM * (1 - 0.55 * load) + g(0, 60)
    rpm_max = min(NO_LOAD_RPM + 120, max(rpm_avg, NO_LOAD_RPM * (1 - 0.25 * load) + abs(g(0, 80))))
    i_avg = 900 + 9500 * load + g(0, 250)
    i_peak = i_avg * rng.uniform(1.15, 1.6) + abs(g(0, 300))
    if cls == 3:                                            # bind: speed collapses at the end, current spikes
        rpm_avg *= rng.uniform(0.45, 0.8)
        i_peak = max(i_peak, rng.uniform(9000, 15000))
        i_avg *= rng.uniform(1.05, 1.4)
    r_int = 0.09                                            # pack plus wiring, ohm
    v_min = pack_mv - i_peak * r_int * rng.uniform(0.8, 1.1) + g(0, 60)
    temp_c = temp_c + (i_avg / 1000) ** 2 * dur / 1000 * 0.012 + g(0, 0.2)
    f = dict(dur_ms=int(dur), rpm_avg=int(max(0, rpm_avg)), rpm_max=int(rpm_max), i_avg_ma=int(max(0, i_avg)),
             i_peak_ma=int(max(0, i_peak)), v_min_mv=int(max(0, v_min)), temp_c=int(round(temp_c)))
    return f, temp_c


def cool(temp_c: float, idle_s: float, ambient: float = 24.0):
    """Newton cooling between pulls, about a 4 minute time constant."""
    return ambient + (temp_c - ambient) * (0.9958 ** idle_s)


def task_mix(rng: random.Random, profile: str = "mixed"):
    """Which class the next pull is. Profiles let a simulated crew differ (a framing crew drives more heavy pulls)."""
    weights = {"mixed": (0.35, 0.40, 0.18, 0.07), "finish": (0.6, 0.35, 0.04, 0.01),
               "framing": (0.15, 0.35, 0.40, 0.10)}[profile]
    return rng.choices(range(4), weights=weights)[0]

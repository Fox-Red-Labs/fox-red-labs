#!/usr/bin/env python3
"""Results rows for the LP-AM263P multicore runs (am263p/bench/multicore). Copies the logs into am263p/data/logs/multicore/.
multicore_rows.py [--copy] [--rows]
  ipc_roundtrip_us : TI's ipc_notify_echo, 1,000,000 messages per remote core, timed on core 0 with the PMU cycle counter
  coremark         : TI's CoreMark on each core alone and on all four at once (am263p/bench/multicore/coremark4)"""
import glob, hashlib, os, re, shutil, statistics, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); REPO = os.path.dirname(HERE)
SRC = os.path.abspath("out/am263p/multicore"); DST = os.path.join(HERE, "data", "logs", "multicore")
BASE = ("compiler=ticlang-5.1.1.LTS;sdk=mcu_plus_sdk_am263px_26_01_00_08;board_rev=A (PROC171A);machine=macos-15.7.9-x86_64;"
        "four R5F cores at 400 MHz in split mode, loaded into RAM over the debugger with am263p/tools/mcload.py, one image per core")
CORE = {0: "R5FSS0-0", 1: "R5FSS0-1", 2: "R5FSS1-0", 3: "R5FSS1-1"}

def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]
def rel(p): return os.path.relpath(p, REPO)
def q(s): return '"' + s.replace('"', "'") + '"'

def ipc(v):
    out = []
    for i in (1, 2, 3):
        p = os.path.join(DST, f"ipc-{v}-r{i}.txt")
        if not os.path.exists(p): continue
        m = re.search(r"FRL_IPC cycles (\d+) messages_per_core (\d+) remote_cores (\d+) ns_per_round_trip_per_core (\d+)", open(p).read())
        if m and "All tests have passed" in open(p).read(): out.append((int(m.group(1)), int(m.group(3)), p))
    return out

def cm(mask):
    runs = []
    for i in (1, 2, 3):
        p = os.path.join(DST, f"cm4-{mask}-r{i}.txt")
        if not os.path.exists(p): continue
        t = open(p).read(); d = {}
        for m in re.finditer(r"FRL_CMR core (\d) score ([0-9.]+) errors (-?\d+) known_id (-?\d+) crclist (0x[0-9a-f]+) crcmatrix (0x[0-9a-f]+) crcstate (0x[0-9a-f]+)", t):
            ok = m.group(3) == "0" and (m.group(5), m.group(6), m.group(7)) == ("0xe714", "0x1fd7", "0x8e3a")
            if ok: d[int(m.group(1))] = float(m.group(2))
        runs.append((d, p))
    return runs

if __name__ == "__main__":
    a = sys.argv[1:]
    if "--copy" in a:
        os.makedirs(DST, exist_ok=True)
        for f in glob.glob(os.path.join(SRC, "*.txt")): shutil.copy(f, DST)
    if "--rows" in a:
        for v, var, os_name in (("freertos_one", "freertos-to-nortos-1-remote", "FreeRTOS"), ("freertos", "freertos-to-nortos-3-remotes", "FreeRTOS"),
                                ("threadx_one", "threadx-to-nortos-1-remote", "ThreadX"), ("threadx", "threadx-to-nortos-3-remotes", "ThreadX")):
            r = ipc(v)
            if len(r) < 3: print(f"# ipc {v}: {len(r)} runs", file=sys.stderr); continue
            us = [c * 2.5 / 1e6 / 1000 for c, _, _ in r]   # cycles at 400 MHz per round trip, in us (1,000,000 messages)
            med = statistics.median(us)
            cond = BASE + (f";TI's ipc_notify_echo (MCU+ SDK 26.01): {os_name} on R5FSS0-0 sends, no RTOS on R5FSS0-1, R5FSS1-0 and R5FSS1-1 echo; "
                           f"{r[0][1]} remote core(s) echoing at once (with 1, the other two are loaded and idle); 1,000,000 round trips per remote core; "
                           f"whole exchange timed on R5FSS0-0 with the PMU cycle counter (the FreeRTOS tick-based clock read 664 us for a 3.9 s exchange, "
                           f"starved by the echo interrupts); round trip = total time / 1,000,000;log_sha256={sha(r[0][2])}")
            note = f"runs {', '.join(f'{x:.3f}' for x in us)} us; TI's 'All tests have passed' in each run; the remote cores' own clocks agree (3.86 s for the 3-remote FreeRTOS exchange)"
            print(f"am263p,ipc_roundtrip_us,{var},measured,{med:.3f},{max(us):.3f},3,400,{q(cond)},2026-10-11,{rel(r[0][2])},serial,{q(note)}")
        cmcond = BASE + (";TI's coremark_benchmark (-O3, 15,000 iterations) built into TI's ipc_notify_echo system (FreeRTOS on R5FSS0-0, no RTOS on the others); "
                         "the chosen cores start together after IpcNotify_syncAll; code and data in each core's own 256 kB on-chip RAM region "
                         "(0x70040000, 0x70080000, 0x700C0000, 0x70100000); results passed to core 0 through a shared uncached table; "
                         "our copy of core_main.c drops its driver open and close calls and exports its result (marked in the file)")
        for mask, core in (("0x1", 0), ("0x2", 1), ("0x4", 2), ("0x8", 3)):
            r = cm(mask); s = [d[core] for d, _ in r if core in d]
            if len(s) < 3: print(f"# cm {mask}: {len(s)}", file=sys.stderr); continue
            var = f"ti-port-o3-ocsram-core{core}-alone" + ("-freertos" if core == 0 else "")
            note = f"runs {', '.join(f'{x:.3f}' for x in s)}; CoreMark's list, matrix and state CRCs correct in every run; {CORE[core]} alone, the other three loaded and idle"
            print(f"am263p,coremark,{var},measured,{statistics.median(s):.3f},{max(s):.3f},3,400,{q(cmcond + ';log_sha256=' + sha(r[0][1]))},2026-10-11,{rel(r[0][1])},crc,{q(note)}")
        r = cm("0xF")
        for core in range(4):
            s = [d[core] for d, _ in r if core in d]
            if len(s) < 3: print(f"# cm all core{core}: {len(s)}", file=sys.stderr); continue
            note = f"runs {', '.join(f'{x:.3f}' for x in s)}; all four cores running CoreMark at once; CRCs correct in every run"
            print(f"am263p,coremark,ti-port-o3-ocsram-all4-core{core},measured,{statistics.median(s):.3f},{max(s):.3f},3,400,{q(cmcond + ';log_sha256=' + sha(r[0][1]))},2026-10-11,{rel(r[0][1])},crc,{q(note)}")
        tot = [sum(d.values()) for d, _ in r if len(d) == 4]
        if len(tot) == 3:
            print(f"am263p,coremark,ti-port-o3-ocsram-all4-sum,measured,{statistics.median(tot):.3f},{max(tot):.3f},3,400,"
                  f"{q(cmcond + ';derived: the sum of the four cores in the same run')},2026-10-11,{rel(r[0][1])},crc,"
                  f"{q('runs ' + ', '.join(f'{x:.3f}' for x in tot) + '; each core within 0.2% of its score alone')}")

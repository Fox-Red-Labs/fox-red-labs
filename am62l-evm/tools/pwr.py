#!/usr/bin/env python3
# usage: pwr.py SECONDS  -> mean per rail (mV, mA, mW) over SECONDS from INA228s on i2c-1
import os, fcntl, sys, time
RAILS = [("VDD_CORE",0x40,0.020),("DVDD3V3",0x4C,0.100),("DVDD1V8",0x45,0.100),("VDDA1V8",0x4D,0.100),
         ("LPDDR4_PMIC1",0x47,0.040),("LPDDR4_PMIC2",0x49,0.040),("VDD_RTC",0x44,1.0),("VDDS_RTC1V8",0x46,1.0)]
fd = os.open('/dev/i2c-1', os.O_RDWR)
def rd(addr, reg, n):
    fcntl.ioctl(fd, 0x0703, addr); os.write(fd, bytes([reg])); return int.from_bytes(os.read(fd, n), 'big')
def s20(v):
    v >>= 4
    return v - (1<<20) if v & 0x80000 else v
T = float(sys.argv[1]); acc = {r[0]:[0,0,0] for r in RAILS}; n = 0; t0 = time.time()
rng = {}
for name, a, r in RAILS:
    fcntl.ioctl(fd, 0x0703, a); os.write(fd, bytes([0])); cfg = int.from_bytes(os.read(fd,2),'big'); rng[name] = 78.125e-9 if cfg & 0x10 else 312.5e-9
while time.time() - t0 < T:
    for name, a, r in RAILS:
        vs = s20(rd(a, 4, 3)) * rng[name]; vb = (rd(a, 5, 3) >> 4) * 195.3125e-6
        i = vs / r; acc[name][0] += vb; acc[name][1] += i; acc[name][2] += vb * i
    n += 1; time.sleep(0.02)
tot = 0
for name, a, r in RAILS:
    v, i, p = (x / n for x in acc[name]); tot += p
    print(f"{name:13s} {v*1000:7.0f} mV {i*1000:8.2f} mA {p*1000:8.1f} mW")
print(f"TOTAL_mW {tot*1000:.1f} samples {n} seconds {time.time()-t0:.1f}")

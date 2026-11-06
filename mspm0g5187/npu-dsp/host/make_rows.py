#!/usr/bin/env python3
"""Writes RESULTS-ROWS.csv from the numbers in results/runall.txt and results/accuracy-report.json (typed in below from those files)."""
import csv
rows=[]
H="board,test,variant,status,value,value_hi,runs,clock_mhz,conditions,date,source,checked,note".split(",")
D="2026-10-20"; SRC="mspm0g5187/npu-dsp/results/runall.txt"
cond="clock 80 MHz SYSPLL; TI Arm Clang 5.1.1.LTS -O2 harness; NNC 2.1.2 -Os; board LP-MSPM0G5187 MG020001; code in flash; skip_normalize and output_int so int8 in and out; median of 20 calls after a checked first call; one erase and flash and run per image; timing boundary: tvmgen_default_run to tvmgen_default_finished (NPU) or return (CPU)"
def add(test,var,val="",hi="",runs=1,status="measured",checked="vectors",note="",cond_=cond,src=SRC,clk=80):
    rows.append(dict(board="mspm0g5187",test=test,variant=var,status=status,value=val,value_hi=hi,runs=runs,clock_mhz=clk,conditions=cond_,date=D,source=src,checked=checked,note=note))
T=[("fir64","256 outputs x 4 channels x 64 taps",0.5659,16.8238),("fir64_dec4","64 outputs x 4 channels x 64 taps, stride 4",0.1849,4.3037),
   ("bank8","256 outputs x 8 channels x 64 taps",0.9673,36.7128),("bank8_trip","bank8 + max pool 256 + comparator",0.6773,33.2731),
   ("bank8_trip2","bank8 + max pool 16 twice + comparator",0.6943,33.3183),("bank8_trip3","bank8 + max pool 16 + comparator",0.7129,33.6684),
   ("bank8_trips","bank8 + max pool 256 + signed comparator",0.6773,33.2739),("dft64","64-point DFT as 64x64 fully connected",0.1951,0.8298)]
for m,desc,n,c in T:
    add("npu_dsp_block_ms",f"{m}-nnc-npu",n,note=f"{desc}; output equals the integer reference byte for byte")
    add("npu_dsp_block_ms",f"{m}-nnc-cpu",c,note=f"{desc}; TI NNC type=soft on the Cortex-M0+")
    add("npu_dsp_speedup_x",f"{m}-npu-vs-nnc-cpu",round(c/n,1),status="computed",checked="by-us",note=f"{c} ms over {n} ms",cond_="derived from the two block_ms rows above")
add("npu_dsp_block_ms","fir64-c-hand-1ch",3.2864,note="hand-written int8 loop, plain C -O3, ONE channel (cutoff 0.1 fs), 256 outputs x 64 taps, 16.0 cycles per tap; matches reference")
add("npu_dsp_block_ms","fir64-c-hand-4ch",12.2242,note="hand-written int8 loop, plain C -O3, four channels; matches reference")
add("npu_dsp_block_ms","bank8-c-hand-8ch",22.7415,note="hand-written int8 loop, plain C -O3, eight channels; matches reference")
add("npu_dsp_speedup_x","fir64-npu-vs-c-hand-4ch",21.6,status="computed",checked="by-us",note="12.2242 ms over 0.5659 ms",cond_="derived from block_ms rows")
add("npu_dsp_speedup_x","fir64-npu4ch-vs-c-hand-1ch",5.8,status="computed",checked="by-us",note="3.2864 ms (one channel) over 0.5659 ms (four channels; the NPU cannot run fewer than four)",cond_="derived from block_ms rows")
add("npu_dsp_speedup_x","bank8-npu-vs-c-hand-8ch",23.5,status="computed",checked="by-us",note="22.7415 ms over 0.9673 ms",cond_="derived from block_ms rows")
A="mspm0g5187/npu-dsp/results/accuracy-report.json"
for ch,(fc,w,t) in enumerate([(0.10,44.27,34.73),(0.05,53.53,34.61),(0.15,42.65,35.10),(0.20,47.00,35.39)]):
    add("npu_dsp_snr_db",f"fir64-ch{ch}-total",t,status="computed",checked="by-us",note=f"cutoff {fc} fs; int8 pipeline against float taps in float arithmetic on the same int8 input; the NPU output equals the integer reference byte for byte",cond_="Python (host/make_models.py) on the test signal",src=A,clk="")
    add("npu_dsp_snr_db",f"fir64-ch{ch}-weights-only",w,status="computed",checked="by-us",note=f"cutoff {fc} fs; int8 taps in float arithmetic with no output rounding, against float taps",cond_="Python (host/make_models.py) on the test signal",src=A,clk="")
add("npu_dsp_match","16-images-vs-reference",16,hi=16,note="16 of 16 images (8 graphs, each on the NPU and as NNC CPU code): 0 output bytes differ from the integer reference, maximum difference 0")
for m,fl,rm in [("fir64",8072,4976),("fir64_dec4",6216,3440),("bank8",8480,8304),("bank8_trip",7064,6268),("dft64",10096,2700)]:
    add("npu_dsp_image_bytes",f"{m}-nnc-npu-flash",fl,note="text of the whole image incl. harness and NPU library",cond_="tiarmsize on dsp.out",checked="linker",src="mspm0g5187/npu-dsp/build.sh",clk="")
    add("npu_dsp_image_bytes",f"{m}-nnc-npu-ram",rm,note="data plus bss, includes the 2048 B stack and the int8 in and out buffers",cond_="tiarmsize on dsp.out",checked="linker",src="mspm0g5187/npu-dsp/build.sh",clk="")
add("npu_dsp_image_bytes","bank8-nnc-cpu-ram",20940,note="the NNC CPU build keeps a 32-bit work buffer; still fits 32 kB",cond_="tiarmsize on dsp.out",checked="linker",src="mspm0g5187/npu-dsp/build.sh",clk="")
for t,why in [("npu_dsp_energy_uj","not run: this wave measured time only, no EnergyTrace run"),("npu_dsp_streaming_overlap","not run: the 63-sample overlap between blocks was not built into a streaming loop"),("npu_dsp_cpu_free_fraction","not run: the NPU harness spins while waiting; a task running beside the NPU was not measured"),("npu_dsp_am13e230","not run: the LP-AM13E230 was off limits this wave"),("npu_dsp_repeat_flashes","not run: one erase, flash and run per image (20 calls each); repeat flashes were skipped because the probe lock was shared and each flash waited minutes")]:
    add(t,"",status="not_run",checked="",note=why,cond_="",src="mspm0g5187/npu-dsp/README.md",clk="")
w=csv.DictWriter(open("RESULTS-ROWS.csv","w",newline=""),H); w.writeheader(); w.writerows(rows); print(len(rows))

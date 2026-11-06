#!/usr/bin/env python3
"""TVM/DLR models from TI's precompiled zoo on the SK-TDA4VM: load the compiled artifacts with the DLR runtime
(TIDL subgraph on the C7x/MMA), random uint8 input of the shape in param.yaml, one warm-up, N timed runs, median ms.
Same method as zoo_bench.py used for the ONNX Runtime and TFLite rows. Usage: tvm_bench.py [runs]"""
import sys, time, os, glob, statistics, yaml, numpy as np
N=int(sys.argv[1]) if len(sys.argv)>1 else 30
import dlr
print("dlr", dlr.__version__, flush=True)
for d in sorted(glob.glob("/opt/model_zoo/TVM-*")):
    p=yaml.safe_load(open(os.path.join(d,"param.yaml")))
    sess=p["session"]; art=os.path.join(d, sess.get("artifacts_folder","artifacts"))
    ins=sess["input_details"]
    print("==", os.path.basename(d), p.get("task_type"), [(i["name"], i["shape"], i["type"]) for i in ins], flush=True)
    t0=time.time()
    try:
        m=dlr.DLRModel(art, "cpu")
    except Exception as e:
        print("LOAD FAILED:", repr(e), flush=True); continue
    tload=time.time()-t0
    feed={}
    for i in ins:
        dt=np.uint8 if "uint8" in i["type"] else (np.float32 if "float" in i["type"] else np.int8)
        feed[i["name"]]=np.random.randint(0,255,size=i["shape"]).astype(dt) if dt!=np.float32 else np.random.rand(*i["shape"]).astype(np.float32)
    try:
        out=m.run(feed)  # warm-up
    except Exception as e:
        print("RUN FAILED:", repr(e), flush=True); continue
    ts=[]
    for k in range(N):
        a=time.perf_counter(); m.run(feed); ts.append((time.perf_counter()-a)*1000)
    print(f"load_s={tload:.2f} runs={N} median_ms={statistics.median(ts):.2f} min_ms={min(ts):.2f} max_ms={max(ts):.2f} outputs={[np.asarray(o).shape for o in out]}", flush=True)
print("TVM_ALL_DONE")

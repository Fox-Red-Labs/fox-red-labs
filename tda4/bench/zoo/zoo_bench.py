# Time every precompiled model in /opt/model_zoo through its runtime with the TIDL delegate (C7x/MMA), then CPU-only for the ONNX ones.
import os, sys, time, glob, yaml, numpy as np
Z="/opt/model_zoo"; N=int(sys.argv[1]) if len(sys.argv)>1 else 30; CPU=len(sys.argv)>2 and sys.argv[2]=="cpu"
def rss(): return int(open("/proc/self/status").read().split("VmRSS:")[1].split()[0])//1024
for d in sorted(glob.glob(Z+"/*")):
    p=yaml.safe_load(open(d+"/param.yaml")); sess=p["session"]; rt=sess["session_name"]; mp=os.path.join(d,sess["model_path"]) if not os.path.isabs(sess["model_path"]) else sess["model_path"]
    art=os.path.join(d,sess.get("artifacts_folder","artifacts"))
    task=p.get("task_type","?"); name=os.path.basename(d)
    try:
        if rt=="onnxrt":
            import onnxruntime as ort
            so=ort.SessionOptions(); so.intra_op_num_threads=2
            if CPU: s=ort.InferenceSession(mp,so,providers=["CPUExecutionProvider"])
            else: s=ort.InferenceSession(mp,so,providers=["TIDLExecutionProvider","CPUExecutionProvider"],provider_options=[{"artifacts_folder":art,"debug_level":"0"},{}])
            inp=s.get_inputs()[0]; shape=[1 if (x is None or isinstance(x,str)) else x for x in inp.shape]
            dt=np.uint8 if "uint8" in inp.type else np.float32
            x=(np.random.rand(*shape)*255).astype(dt); f=lambda: s.run(None,{inp.name:x})
        elif rt=="tflitert":
            if CPU: continue
            import tflite_runtime.interpreter as tfl
            delegate=tfl.load_delegate("libtidl_tfl_delegate.so",{"artifacts_folder":art,"debug_level":"0"})
            it=tfl.Interpreter(model_path=mp,experimental_delegates=[delegate],num_threads=2); it.allocate_tensors()
            inp=it.get_input_details()[0]; shape=list(inp["shape"]); x=(np.random.rand(*shape)*255).astype(inp["dtype"])
            def f(): it.set_tensor(inp["index"],x); it.invoke()
        else:
            print(f"{name}: runtime {rt} skipped", flush=True); continue
        f(); ts=[]
        n=N if not CPU else 3
        for i in range(n):
            t=time.time(); f(); ts.append((time.time()-t)*1000)
        ts.sort(); print(f"{name} | {task} | {rt} | {'A72-cpu' if CPU else 'tidl-c7x'} | {shape} | median {ts[len(ts)//2]:.2f} ms | min {ts[0]:.2f} | max {ts[-1]:.2f} | n {n} | rss {rss()} MB", flush=True)
    except Exception as e:
        print(f"{name} | {task} | {rt} | FAILED {type(e).__name__}: {str(e)[:120]}", flush=True)

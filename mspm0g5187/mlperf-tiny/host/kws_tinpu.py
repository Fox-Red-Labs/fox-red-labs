#!/usr/bin/env python3
"""TI NPU quantization (tinyml_torchmodelopt, 8-bit weights and activations) of the MLPerf Tiny DS-CNN, then fold the
exported int8 ONNX file (fold.py) and score it with onnxruntime on the 4,890-clip test split.
  python3 kws_tinpu.py ptq <out dir> <calibration clips>        post-training: forward passes only
  python3 kws_tinpu.py qat <out dir> <epochs>                   quantization-aware training on the 85,511 training clips
    environment: KWS_LR (default 1e-4), KWS_AUG=1 (random shift of up to 2 frames in time and Gaussian noise of 3 percent of the
    feature spread on each training batch); with KWS_AUG the epoch kept is the one best on the validation split kws_val.npz,
    never on the test split
Inputs: kws_ref_model_float32.tflite, kws_test.npz, kws_train.npz (prep_kws.py), kws_torch.py, fold.py (tools/byom)."""
import sys, os, json, time, subprocess, numpy as np, torch, onnxruntime as ort
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kws_torch import from_tflite, nchw
from tinyml_torchmodelopt.quantization import TINPUTinyMLPTQFxModule, TINPUTinyMLQATFxModule, TinyMLQConfigType
mode, out, n = sys.argv[1], sys.argv[2], int(sys.argv[3]); os.makedirs(out, exist_ok=True)
torch.manual_seed(0); np.random.seed(0); torch.set_num_threads(8)
TF = os.environ.get("KWS_TFLITE", "kws_ref_model_float32.tflite"); FOLD = os.environ.get("FOLD", "fold.py")
te, tr = np.load("kws_test.npz"), np.load("kws_train.npz"); xte, yte = te["x"], te["y"]; xtr, ytr = nchw(tr["x"]), torch.tensor(tr["y"]).long()
m = from_tflite(TF)
def acc_t(model, k=len(xte)):
    model.eval()
    with torch.no_grad(): return float(np.mean(torch.cat([model(nchw(xte[i:i+500])).argmax(1) for i in range(0, k, 500)]).numpy() == yte[:k]))
fl = acc_t(m); print("float torch", fl, flush=True)
ex = torch.rand(1, 1, 49, 10) * 50 - 50
cfg = TinyMLQConfigType(weight_bitwidth=8, activation_bitwidth=8, auto_quantization=False)
log = []
if mode == "ptq":
    q = TINPUTinyMLPTQFxModule(m, example_inputs=ex, total_epochs=1, qconfig_type=cfg); q.train()
    idx = torch.randperm(len(xtr))[:n]
    with torch.no_grad():
        for i in range(0, n, 64): q(xtr[idx[i:i+64]])
else:
    q = TINPUTinyMLQATFxModule(m, example_inputs=ex, total_epochs=n, qconfig_type=cfg)
    LR = float(os.environ.get("KWS_LR", "1e-4")); AUG = os.environ.get("KWS_AUG") == "1"; import copy
    va = np.load("kws_val.npz"); xva, yva = va["x"], va["y"]; sd = float(xtr.std()); best = (-1, None)
    def acc_v(model):
        model.eval()
        with torch.no_grad(): return float(np.mean(torch.cat([model(nchw(xva[i:i+500])).argmax(1) for i in range(0, len(xva), 500)]).numpy() == yva))
    opt = torch.optim.Adam(q.parameters(), LR); sch = torch.optim.lr_scheduler.CosineAnnealingLR(opt, n); lossf = torch.nn.CrossEntropyLoss()
    for e in range(n):
        q.train(); t0 = time.time(); perm = torch.randperm(len(xtr))
        for i in range(0, len(xtr), 128):
            b = perm[i:i+128]; xb = xtr[b]
            if AUG: xb = torch.roll(xb, int(np.random.randint(-2, 3)), dims=2) + 0.03 * sd * torch.randn_like(xb)
            opt.zero_grad(); l = lossf(q(xb), ytr[b]); l.backward(); opt.step()
        sch.step(); a = acc_t(q, 1000); log.append(dict(epoch=e + 1, seconds=round(time.time() - t0, 1), loss=float(l), acc_first1000=a, acc_val=acc_v(q))); print(log[-1], flush=True)
        if AUG and log[-1]["acc_val"] > best[0]: best = (log[-1]["acc_val"], copy.deepcopy(q.state_dict()))
    if AUG: q.load_state_dict(best[1]); print("kept the epoch with validation", best[0], flush=True)
q.eval(); qa = acc_t(q); print("fake-quant torch", qa, flush=True)
qc = q.convert(); raw, fin = f"{out}/kws_{mode}_unsimplified.onnx", f"{out}/kws_{mode}_int8.onnx"
qc.export(ex, raw, input_names=["input"], simplify=False)
subprocess.run([sys.executable, FOLD, raw, fin], check=True)
s = ort.InferenceSession(fin); nm = s.get_inputs()[0].name
p = np.concatenate([s.run(None, {nm: xte[i:i+1].transpose(0, 3, 1, 2).astype(np.float32)})[0].reshape(1, -1).argmax(1) for i in range(len(xte))])
r = dict(mode=mode, n=n, float_acc=fl, fakequant_acc=qa, onnx_int8_acc=float((p == yte).mean()), log=log)
json.dump(r, open(f"{out}/kws_{mode}_result.json", "w"), indent=1); print(json.dumps({k: v for k, v in r.items() if k != "log"}))

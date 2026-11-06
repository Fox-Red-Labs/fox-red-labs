#!/usr/bin/env python3
"""MLPerf Tiny keyword-spotting DS-CNN rebuilt in PyTorch from mlcommons/tiny kws_ref_model_float32.tflite
(BatchNorm already folded into the conv biases there; the file keeps the weights as int8 with a scale per channel). Input: the 49x10 MFCC the reference get_dataset.py makes, NCHW.
TF 'same' padding is bottom/right heavy, so the first conv gets an explicit pad (top 4, bottom 5, left 1, right 1).
  python3 kws_torch.py check <float32.tflite> <kws_test.npz>     torch vs TFLite interpreter, accuracy on the test split
"""
import sys, numpy as np, torch, torch.nn as nn

class DSCNN(nn.Module):
    def __init__(s, f=64, n=12):
        super().__init__()
        s.pad = nn.ZeroPad2d((1, 1, 4, 5)); s.c0 = nn.Conv2d(1, f, (10, 4), (2, 2)); s.r0 = nn.ReLU()
        s.dw = nn.ModuleList([nn.Conv2d(f, f, 3, 1, 1, groups=f) for _ in range(4)])
        s.pw = nn.ModuleList([nn.Conv2d(f, f, 1) for _ in range(4)])
        s.rd = nn.ModuleList([nn.ReLU() for _ in range(4)]); s.rp = nn.ModuleList([nn.ReLU() for _ in range(4)])
        s.pool = nn.AvgPool2d((25, 5)); s.fc = nn.Linear(f, n)
    def forward(s, x):
        x = s.r0(s.c0(s.pad(x)))
        for d, p, a, b in zip(s.dw, s.pw, s.rd, s.rp): x = b(p(a(d(x))))
        return s.fc(torch.flatten(s.pool(x), 1))

def from_tflite(path):
    import tensorflow as tf
    it = tf.lite.Interpreter(path); it.allocate_tensors()
    convs, dws, fc = [], [], None
    td = {t["index"]: t for t in it.get_tensor_details()}
    def get(i):
        """The 'float32' file of mlcommons/tiny stores int8 weights with a scale per output channel: dequantize."""
        a = it.get_tensor(i)
        if a.dtype == np.float32: return a
        q = td[i]["quantization_parameters"]; sc, zp, d = q["scales"], q["zero_points"], q["quantized_dimension"]
        sh = [1] * a.ndim; sh[d] = -1
        return (a.astype(np.float32) - zp.reshape(sh)) * sc.reshape(sh) if len(sc) > 1 else (a.astype(np.float32) - zp[0]) * sc[0]
    for op in it._get_ops_details():
        n = op["op_name"]
        if n in ("CONV_2D", "DEPTHWISE_CONV_2D", "FULLY_CONNECTED"):
            w, b = get(op["inputs"][1]), get(op["inputs"][2])
            {"CONV_2D": convs, "DEPTHWISE_CONV_2D": dws, "FULLY_CONNECTED": [None]}[n].append((w, b)) if n != "FULLY_CONNECTED" else None
            if n == "FULLY_CONNECTED": fc = (w, b)
    m = DSCNN(); assert len(convs) == 5 and len(dws) == 4, (len(convs), len(dws))
    with torch.no_grad():
        w, b = convs[0]; m.c0.weight.copy_(torch.tensor(w.transpose(0, 3, 1, 2))); m.c0.bias.copy_(torch.tensor(b))
        for i in range(4):
            w, b = dws[i]; m.dw[i].weight.copy_(torch.tensor(w.transpose(3, 0, 1, 2).copy())); m.dw[i].bias.copy_(torch.tensor(b))
            w, b = convs[i + 1]; m.pw[i].weight.copy_(torch.tensor(w.transpose(0, 3, 1, 2))); m.pw[i].bias.copy_(torch.tensor(b))
        m.fc.weight.copy_(torch.tensor(fc[0])); m.fc.bias.copy_(torch.tensor(fc[1]))
    return m.eval()

def nchw(x): return torch.tensor(np.asarray(x, np.float32).transpose(0, 3, 1, 2))

if __name__ == "__main__" and sys.argv[1] == "check":
    import tensorflow as tf
    m = from_tflite(sys.argv[2]); D = np.load(sys.argv[3]); x, y = D["x"], D["y"]
    it = tf.lite.Interpreter(sys.argv[2]); it.allocate_tensors(); i0, o0 = it.get_input_details()[0], it.get_output_details()[0]
    d = 0.0
    for k in range(200):
        it.set_tensor(i0["index"], x[k:k+1].astype(np.float32)); it.invoke()
        with torch.no_grad(): t = m(nchw(x[k:k+1])).numpy()
        d = max(d, float(np.abs(torch.softmax(torch.tensor(t), 1).numpy() - it.get_tensor(o0["index"])).max()))
    with torch.no_grad(): p = torch.cat([m(nchw(x[i:i+500])).argmax(1) for i in range(0, len(x), 500)])
    print("max abs softmax diff torch vs TFLite (200 clips):", d, " torch float accuracy on %d clips:" % len(x), float((p.numpy() == y).mean()))

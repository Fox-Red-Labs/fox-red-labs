#!/usr/bin/env python3
"""Build the keyword-spotting dataset folder the MLPerf Tiny host runner reads (kws01: tst_NNNNNN_<Word>_<k>.bin files and
y_labels.csv) from the test split in kws_test.npz, the way third-party/mlperf-tiny/kws/make_bin_files.py does: the first
1000 test clips in tfds order, MFCC divided by the reference int8 model's input scale plus its zero point, cast to int8.
Also writes kws_lut.h: for each of the 256 byte values the host sends, the int8 the TI-NPU-quantized model takes
(that model's input step is 1.0 with zero offset: round(x), clipped to -128..127, read from its ONNX file).
  python3 make_kws01.py <kws_test.npz> <kws_ref_model.tflite> <out dir> <model int8 onnx> [clips, default 1000]"""
import sys, os, numpy as np, onnx
from onnx import numpy_helper
import tensorflow as tf
npz, tfl, out, onx = sys.argv[1:5]; N = int(sys.argv[5]) if len(sys.argv) > 5 else 1000
D = np.load(npz); x, y = D["x"][:N], D["y"][:N]
it = tf.lite.Interpreter(tfl); it.allocate_tensors(); s, z = it.get_input_details()[0]["quantization"]
words = ["Down", "Go", "Left", "No", "Off", "On", "Right", "Stop", "Up", "Yes", "Silence", "Unknown"]
os.makedirs(out, exist_ok=True); lines = []
for i in range(N):
    q = np.array(x[i] / s + z, dtype=np.int8); fn = f"tst_{i:06d}_{words[y[i]]}_{y[i]}.bin"
    open(os.path.join(out, fn), "wb").write(q.flatten().tobytes()); lines.append(f"{fn}, 12, {y[i]}\n")
open(os.path.join(out, "y_labels.csv"), "w").writelines(lines)
g = onnx.load(onx).graph; c = {i.name: numpy_helper.to_array(i) for i in g.initializer}
off, mult, sh = [float(c[k].reshape(-1)[0]) for k in ("oss_12.offset", "oss_12.mult", "oss_12.shift_mult")]
# the byte b (0..255) is int8 (b-256 if b>127); table index is the unsigned byte
tab = [0] * 256
for b in range(256):
    q = b - 256 if b > 127 else b; tab[b] = int(np.clip(np.floor((((q - z) * s) + off) * mult * sh), -128, 127))
open(os.path.join(out, "kws_lut.h"), "w").write("/* byte sent by the host runner -> int8 input of the TI-NPU model; scale %.9g zero point %d, model offset %g mult %g shift %g */\n#include <stdint.h>\nstatic const int8_t kws_lut[256] = {%s};\n" % (s, z, off, mult, sh, ",".join(map(str, tab))))
print("input scale", s, "zero point", z, "files", len(lines), "label counts", np.bincount(y, minlength=12).tolist())

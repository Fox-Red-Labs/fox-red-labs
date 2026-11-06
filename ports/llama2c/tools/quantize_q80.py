#!/usr/bin/env python3
"""Turn a llama2.c float checkpoint (version 0, the stories260K.bin layout) into the int8 "version 2" file that
third-party/llama2c/runq.c reads. Same steps as version2_export() in third-party/llama2c/export.py (Q8_0: symmetric int8,
range [-127,127], one float32 scale per group), but reads the float file with numpy, so it needs no PyTorch.
  python3 -I quantize_q80.py stories260K.bin out.bin [group_size]
The group size must divide both dim and hidden_dim. For stories260K (dim 64, hidden_dim 172) that leaves 4:
runq.c rounds hidden_dim down to whole groups, so any larger size would silently drop the tail of the feed-forward vector."""
import struct, sys
import numpy as np

src, dst = sys.argv[1], sys.argv[2]
gs = int(sys.argv[3]) if len(sys.argv) > 3 else 4
raw = open(src, 'rb').read()
dim, hid, nl, nh, nkv, vocab, seq = struct.unpack_from('<7i', raw, 0)
shared = vocab > 0
vocab = abs(vocab)
assert dim % gs == 0 and hid % gs == 0, f"group size {gs} must divide dim {dim} and hidden_dim {hid}"
hs = dim // nh
kvd = dim * nkv // nh
f = np.frombuffer(raw, dtype='<f4', offset=28)
pos = 0
def take(*shape):
    global pos
    n = int(np.prod(shape)); a = f[pos:pos + n].reshape(shape); pos += n; return a
tok = take(vocab, dim)
rms_att = take(nl, dim)
wq = take(nl, dim, dim); wk = take(nl, kvd, dim); wv = take(nl, kvd, dim); wo = take(nl, dim, dim)
rms_ffn = take(nl, dim)
w1 = take(nl, hid, dim); w2 = take(nl, dim, hid); w3 = take(nl, hid, dim)
rms_final = take(dim)
pos += seq * hs // 2 * 2          # freq_cis_real and freq_cis_imag, not stored in version 2
wcls = tok if shared else take(vocab, dim)
assert pos * 4 + 28 == len(raw), (pos * 4 + 28, len(raw))

def q80(w):
    w = w.astype(np.float32).reshape(-1, gs)
    scale = (np.abs(w).max(axis=1) / np.float32(127.0)).astype(np.float32)
    q = np.round(w / scale[:, None]).astype(np.int8)
    return q, scale

tensors = [tok] + [wq[l] for l in range(nl)] + [wk[l] for l in range(nl)] + [wv[l] for l in range(nl)] + [wo[l] for l in range(nl)] \
        + [w1[l] for l in range(nl)] + [w2[l] for l in range(nl)] + [w3[l] for l in range(nl)] + ([] if shared else [wcls])
out = bytearray()
out += struct.pack('I', 0x616b3432) + struct.pack('i', 2) + struct.pack('7i', dim, hid, nl, nh, nkv, vocab, seq)
out += struct.pack('B', int(shared)) + struct.pack('i', gs)
out += b'\0' * (256 - len(out))
for a in (rms_att, rms_ffn, rms_final): out += np.ascontiguousarray(a, dtype='<f4').tobytes()
maxerr = 0.0
for t in tensors:
    q, s = q80(t)
    out += q.tobytes() + s.astype('<f4').tobytes()
    maxerr = max(maxerr, float(np.abs(q.astype(np.float32) * s[:, None] - t.astype(np.float32).reshape(-1, gs)).max()))
open(dst, 'wb').write(out)
print(f"wrote {dst}: {len(out)} bytes, group size {gs}, max quantization error {maxerr:.6f}")

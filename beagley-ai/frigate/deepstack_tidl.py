#!/usr/bin/env python3
"""A DeepStack-style detection server for Frigate that runs a TI model-zoo detector on the AM67A C7x.

Runs ON the BeagleY-AI (BeagleBoard Edge AI image, onnxruntime 1.15.0 with TIDLExecutionProvider).
Frigate's built-in `deepstack` detector POSTs one JPEG per detection to
  http://<board>:<port>/v1/vision/detection      (multipart form: image=<jpeg>, api_key=<str>)
and reads back
  {"success": true, "predictions": [{"label","confidence","x_min","y_min","x_max","y_max"}, ...]}
sorted by confidence (Frigate stops reading at the first one below 0.4 or with a label it does not know).

  FRL_INFER=tidl|cpu   which onnxruntime provider runs the network (default tidl)
  FRL_MODEL=<dir>      model-zoo folder (default the COCO yolox-nano-lite 416x416)
  FRL_LABELS=<file>    optional Frigate labelmap.txt; labels not in it are dropped here so Frigate's early stop never fires
  FRL_PORT=5100  (not 5000: Frigate's nginx owns 5000 on host networking)
  FRL_BGR=1            feed the network BGR (what the zoo's param.yaml asks for: reverse_channels true)

  GET /stats           medians of the last 200 requests, in ms, split into decode, preprocess, network and postprocess
"""
import os, sys, io, time, json, threading, re
from email.parser import BytesParser
from email.policy import default as _policy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import numpy as np, yaml, cv2

MODEL = os.environ.get("FRL_MODEL", "/opt/model_zoo/ONR-OD-8200-yolox-nano-lite-mmdet-coco-416x416")
INFER = os.environ.get("FRL_INFER", "tidl")
PORT = int(os.environ.get("FRL_PORT", "5100"))
BGR = os.environ.get("FRL_BGR", "1") == "1"
MIN_CONF = float(os.environ.get("FRL_MIN_CONF", "0.3"))

p = yaml.safe_load(open(MODEL + "/param.yaml"))
ds = yaml.safe_load(open(MODEL + "/dataset.yaml"))
id2name = {c["id"]: c["name"] for c in ds["categories"]}
offs = p["metric"]["label_offset_pred"]            # network class index -> COCO category id
names = [id2name.get(offs[i], "?").lower() for i in range(len(offs))]   # the map has an 81st entry (91) with no category
keep = None
if os.environ.get("FRL_LABELS"):                    # Frigate's labelmap.txt: "<index> <name>" per line
    keep = {l.split(None, 1)[1].strip().lower() for l in open(os.environ["FRL_LABELS"]) if len(l.split(None, 1)) == 2}
    keep.add("truck")                               # Frigate's detector maps truck to car

sess = p["session"]
mp = os.path.join(MODEL, sess["model_path"]); art = os.path.join(MODEL, sess["artifacts_folder"])
import onnxruntime as ort
so = ort.SessionOptions(); so.intra_op_num_threads = 2
if INFER == "cpu":
    S = ort.InferenceSession(mp, so, providers=["CPUExecutionProvider"])
else:
    S = ort.InferenceSession(mp, so, providers=["TIDLExecutionProvider", "CPUExecutionProvider"],
                             provider_options=[{"artifacts_folder": art, "debug_level": "0"}, {}])
INP = S.get_inputs()[0]; N = INP.shape[2]           # NCHW square input
LOCK = threading.Lock(); HIST = []

def detect(jpeg):
    t0 = time.perf_counter()
    img = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)    # BGR
    if img is None: raise ValueError("not an image")
    h, w = img.shape[:2]
    t1 = time.perf_counter()
    if (h, w) != (N, N):
        s = min(N / h, N / w)                       # the zoo preprocess: keep the aspect ratio, pad to the corner with 114
        img = cv2.resize(img, (int(w * s), int(h * s)))
    else: s = 1.0
    pad = np.full((N, N, 3), 114, np.uint8); pad[:img.shape[0], :img.shape[1]] = img
    if not BGR: pad = pad[:, :, ::-1]
    x = np.ascontiguousarray(pad.transpose(2, 0, 1)[None]).astype(np.uint8 if "uint8" in INP.type else np.float32)
    t2 = time.perf_counter()
    with LOCK:
        dets, labels = S.run(None, {INP.name: x})
    t3 = time.perf_counter()
    out = []
    for d, l in zip(dets, labels):
        sc = float(d[4])
        if sc < MIN_CONF: continue
        nm = names[int(l)] if 0 <= int(l) < len(names) else None
        if nm is None or (keep is not None and nm not in keep): continue
        x1, y1, x2, y2 = [float(v) / s for v in d[:4]]
        out.append({"label": nm, "confidence": round(sc, 4), "x_min": max(0, int(x1)), "y_min": max(0, int(y1)),
                    "x_max": min(w, int(x2)), "y_max": min(h, int(y2))})
    out.sort(key=lambda o: -o["confidence"])
    t4 = time.perf_counter()
    HIST.append(((t1 - t0) * 1e3, (t2 - t1) * 1e3, (t3 - t2) * 1e3, (t4 - t3) * 1e3)); del HIST[:-200]
    return out, (t3 - t2) * 1e3, (t4 - t0) * 1e3

def stats():
    if not HIST: return {"n": 0}
    a = np.array(HIST)
    r = {k: round(float(np.median(a[:, i])), 2) for i, k in enumerate(["decode_ms", "preprocess_ms", "network_ms", "post_ms"])}
    r["server_ms"] = round(float(np.median(a.sum(1))), 2); r["n"] = len(HIST); r["provider"] = INFER; r["model"] = os.path.basename(MODEL)
    return r

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, obj):
        b = json.dumps(obj).encode(); self.send_response(code)
        self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        self._send(200, stats() if self.path.startswith("/stats") else {"success": True, "server": "frl-deepstack-tidl"})
    def do_POST(self):
        try:
            body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            msg = BytesParser(policy=_policy).parsebytes(b"Content-Type: " + self.headers["Content-Type"].encode() + b"\r\n\r\n" + body)
            jpeg = next(pt.get_payload(decode=True) for pt in msg.iter_parts() if pt.get_param("name", header="content-disposition") == "image")
            preds, net_ms, tot_ms = detect(jpeg)
            self._send(200, {"success": True, "predictions": preds, "inferenceMs": round(net_ms, 2), "processMs": round(tot_ms, 2)})
        except Exception as e:
            self._send(400, {"success": False, "error": str(e)})

if __name__ == "__main__":
    z = np.zeros((N, N, 3), np.uint8); ok, j = cv2.imencode(".jpg", z); detect(j.tobytes()); HIST.clear()   # warm up: the first run loads the network onto the C7x
    print(f"ready: {os.path.basename(MODEL)} on {INFER}, {len(names)} classes, port {PORT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), H).serve_forever()

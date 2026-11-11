"""Pose source for the motion games: camera in, hand positions out.

One interface, three ways to get hands:
  cpu    TI's YOLOX-pose model (ONR-KD-7060, 640x640) through onnxruntime on the CPU. Runs on a Mac or any PC. Development.
  tidl   The same model through onnxruntime's TIDL provider on the BeagleY-AI's C7x/MMA accelerator, using the
         precompiled artifacts folder that ships in /opt/model_zoo. Only on the board.
  mouse  The mouse is a hand. No camera, no model. For tuning the game on a Mac.
  demo   Two scripted hands move in loops. For checking the display and the sound with nothing attached.

Hands are in mirrored, normalized screen coordinates: x 0..1 left to right as the player sees the screen, y 0..1 top to bottom.
"""
import glob
import math
import os
import threading
import time

import numpy as np

# COCO keypoint order used by the YOLOX-pose model
L_ELBOW, R_ELBOW, L_WRIST, R_WRIST = 7, 8, 9, 10
NOSE = 0
MODEL_NAME = "ONR-KD-7060-human-pose-yolox-s-640x640"
MODEL_SEARCH = [
    os.environ.get("POSE_MODEL", ""),
    "/opt/model_zoo/" + MODEL_NAME,
    os.path.expanduser("<scratch>/runs/arrivals/beagley-ai/ONR-KD-7060"),
]


def find_model_dir():
    for d in MODEL_SEARCH:
        if d and glob.glob(os.path.join(d, "model", "*.onnx")):
            return d
    raise FileNotFoundError("pose model not found; set POSE_MODEL to a folder with model/*.onnx and artifacts/ (see the arrival plan)")


def letterbox_corner(bgr, size=640):
    """Scale to fit, pad right and bottom with 114 (the model's preprocess in param.yaml: resize_with_pad corner). Returns (rgb_nchw_float, scale)."""
    import cv2
    h, w = bgr.shape[:2]
    s = size / max(h, w)
    nw, nh = int(round(w * s)), int(round(h * s))
    img = cv2.resize(bgr, (nw, nh), interpolation=cv2.INTER_LINEAR)
    canvas = np.full((size, size, 3), 114, np.uint8)
    canvas[:nh, :nw] = img
    rgb = canvas[:, :, ::-1]  # reverse_channels: true
    return np.ascontiguousarray(rgb.transpose(2, 0, 1)[None], dtype=np.float32), s


def _iou(a, b):
    ix = max(0.0, min(a[2], b[2]) - max(a[0], b[0]))
    iy = max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    ua = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter
    return inter / ua if ua > 0 else 0.0


def decode(det, scale, min_score=0.35, nms_iou=0.5):
    """det: (N, 57) = x1 y1 x2 y2 score label then 17 keypoints of (x, y, conf), in 640x640 model pixels.
    The CPU/ONNX output has no non-maximum suppression (measured: 17 rows for 3 people) and keypoint confidences as raw
    logits; both are handled here, so the same code is safe on the TIDL path, which may already have done them.
    Returns people sorted by score: {"score", "box", "kp": (17, 3) in source-frame pixels}."""
    det = np.asarray(det, np.float32)
    det = det.reshape(-1, det.shape[-1])
    det = det[det[:, 4] >= min_score]
    det = det[np.argsort(-det[:, 4])]
    people = []
    for row in det:
        if any(_iou(row[:4], p["box_m"]) > nms_iou for p in people):
            continue
        kp = row[6:57].reshape(17, 3).copy()
        if kp[:, 2].max() > 1.0 or kp[:, 2].min() < 0.0:
            kp[:, 2] = 1.0 / (1.0 + np.exp(-kp[:, 2]))
        kp[:, 0] /= scale
        kp[:, 1] /= scale
        people.append({"score": float(row[4]), "box_m": row[:4].copy(), "box": row[:4] / scale, "kp": kp})
    return people


def hands_from_people(people, frame_w, frame_h, kp_conf=0.3, reach=0.3, max_people=4):
    """Hand points in mirrored 0..1 coordinates. The hand sits a little beyond the wrist along the forearm (reach x forearm length)."""
    hands = []
    for pi, p in enumerate(people[:max_people]):
        kp = p["kp"]
        for wi, ei, name in ((L_WRIST, L_ELBOW, "L"), (R_WRIST, R_ELBOW, "R")):
            wx, wy, wc = kp[wi]
            if wc < kp_conf:
                continue
            x, y = wx, wy
            ex, ey, ec = kp[ei]
            if ec >= kp_conf:
                x += reach * (wx - ex)
                y += reach * (wy - ey)
            hands.append({"id": "%d%s" % (pi, name), "x": float(1.0 - x / frame_w), "y": float(y / frame_h), "conf": float(wc)})
    return hands


class YoloxPose:
    def __init__(self, provider="cpu", model_dir=None, threads=4):
        import onnxruntime as ort
        self.dir = model_dir or find_model_dir()
        onnx = glob.glob(os.path.join(self.dir, "model", "*.onnx"))[0]
        so = ort.SessionOptions()
        so.intra_op_num_threads = threads
        if provider == "tidl":
            art = os.path.join(self.dir, "artifacts")
            self.sess = ort.InferenceSession(onnx, so, providers=["TIDLExecutionProvider", "CPUExecutionProvider"],
                                             provider_options=[{"artifacts_folder": art, "debug_level": "0"}, {}])
            if "TIDLExecutionProvider" not in self.sess.get_providers():
                raise RuntimeError("TIDLExecutionProvider did not load: %s" % self.sess.get_providers())
        else:
            self.sess = ort.InferenceSession(onnx, so, providers=["CPUExecutionProvider"])
        self.inp = self.sess.get_inputs()[0].name
        self.provider = provider

    def __call__(self, bgr):
        x, s = letterbox_corner(bgr)
        det = self.sess.run(None, {self.inp: x})[0]
        return decode(det, s)


class Capture:
    """Camera or video file. A file is paced to its own frame rate and loops, so a recorded clip behaves like a live camera."""

    def __init__(self, src, width=1280, height=720, fps=30):
        import cv2
        self.cv2 = cv2
        self.is_file = isinstance(src, str) and os.path.isfile(src)
        if self.is_file:
            self.cap = cv2.VideoCapture(src)
            self.period = 1.0 / (self.cap.get(cv2.CAP_PROP_FPS) or 30.0)
        else:
            self.period = 0
            if isinstance(src, str) and src.isdigit():
                src = int(src)
            self.cap = cv2.VideoCapture(src, cv2.CAP_V4L2) if (isinstance(src, str) and src.startswith("/dev/")) else cv2.VideoCapture(src)
            if not self.cap.isOpened() and isinstance(src, str) and src.startswith("/dev/"):
                # second try: through GStreamer (the board's OpenCV may be built without V4L2 capture)
                gp = ("v4l2src device=%s ! image/jpeg,width=%d,height=%d,framerate=%d/1 ! jpegdec ! videoconvert ! "
                      "video/x-raw,format=BGR ! appsink drop=true max-buffers=1" % (src, width, height, fps))
                self.cap = cv2.VideoCapture(gp, cv2.CAP_GSTREAMER)
            if self.cap.isOpened():
                self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
                self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
                self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
                self.cap.set(cv2.CAP_PROP_FPS, fps)
                self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        if not self.cap.isOpened():
            raise RuntimeError("cannot open the camera or video: %r" % (src,))
        self._next = time.time()

    def read(self):
        if self.is_file:
            self._next += self.period
            wait = self._next - time.time()
            if wait > 0:
                time.sleep(wait)
            elif wait < -1:
                self._next = time.time()
        ok, f = self.cap.read()
        if not ok and self.is_file:
            self.cap.set(self.cv2.CAP_PROP_POS_FRAMES, 0)
            ok, f = self.cap.read()
        return f if ok else None


class PoseWorker(threading.Thread):
    """Reads frames and runs the model as fast as it can; the game reads the newest result whenever it draws."""

    def __init__(self, kind, cam=None, model_dir=None):
        super().__init__(daemon=True)
        self.kind = kind
        self.lock = threading.Lock()
        self.latest = {"frame": None, "hands": [], "people": [], "t": 0.0, "seq": 0}
        self.infer_ms = 0.0
        self.fps = 0.0
        self.stop_flag = False
        self.mouse = (0.5, 0.5)
        self.cap = Capture(cam) if kind in ("cpu", "tidl") else None
        self.model = YoloxPose(kind, model_dir) if kind in ("cpu", "tidl") else None

    def run(self):
        n, t0, seq = 0, time.time(), 0
        while not self.stop_flag:
            frame, people, hands = None, [], []
            if self.kind in ("cpu", "tidl"):
                frame = self.cap.read()
                if frame is None:
                    time.sleep(0.01)
                    continue
                t = time.time()
                people = self.model(frame)
                self.infer_ms = 0.8 * self.infer_ms + 0.2 * (time.time() - t) * 1000 if self.infer_ms else (time.time() - t) * 1000
                hands = hands_from_people(people, frame.shape[1], frame.shape[0])
            elif self.kind == "mouse":
                time.sleep(1 / 60)
                hands = [{"id": "0M", "x": self.mouse[0], "y": self.mouse[1], "conf": 1.0}]
            else:  # demo
                time.sleep(1 / 30)
                tt = time.time()
                hands = [{"id": "0L", "x": 0.5 + 0.35 * math.sin(tt * 0.7), "y": 0.5 + 0.3 * math.sin(tt * 1.1), "conf": 1.0},
                         {"id": "0R", "x": 0.5 + 0.3 * math.cos(tt * 0.5), "y": 0.55 + 0.3 * math.sin(tt * 0.9 + 1), "conf": 1.0}]
            seq += 1
            n += 1
            if time.time() - t0 >= 1.0:
                self.fps = n / (time.time() - t0)
                n, t0 = 0, time.time()
            with self.lock:
                self.latest = {"frame": frame, "hands": hands, "people": people, "t": time.time(), "seq": seq}

    def get(self):
        with self.lock:
            return self.latest

    def stop(self):
        self.stop_flag = True

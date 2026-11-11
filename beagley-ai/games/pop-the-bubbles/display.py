"""Where the picture goes. Pick with --display:
  window  an OpenCV window (a Mac, or a board with a desktop). q or Esc quits, m mutes.
  mjpeg   a tiny web server on port 8090: open http://<address>:8090/ in any browser. Works on the board with no display attached.
  fb      writes straight to the Linux framebuffer /dev/fb0 (the board's HDMI when no other program holds it).
  gst     GStreamer appsrc into kmssink (TI's own HDMI path on the Edge AI image).
  file    writes an MJPEG .avi (for tests and for the project page).
  none    nothing (timing runs).
"""
import os
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import numpy as np


class NoDisplay:
    mouse = None

    def show(self, frame): pass
    def poll_key(self): return -1
    def close(self): pass


class Window(NoDisplay):
    def __init__(self, w, h):
        import cv2
        self.cv2 = cv2
        cv2.namedWindow("Pop the bubbles", cv2.WINDOW_NORMAL)
        cv2.resizeWindow("Pop the bubbles", w, h)
        cv2.setMouseCallback("Pop the bubbles", self._mouse)
        self._key = -1

    def _mouse(self, ev, x, y, flags, param):
        self.mouse = (x, y)

    def show(self, frame):
        self.cv2.imshow("Pop the bubbles", frame)
        self._key = self.cv2.waitKey(1) & 0xFF

    def poll_key(self):
        k, self._key = self._key, -1
        return k if k != 255 else -1

    def close(self):
        self.cv2.destroyAllWindows()


class Mjpeg(NoDisplay):
    PAGE = (b"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Pop the bubbles</title>"
            b"<body style='margin:0;background:#111'><img src=/stream style='width:100%;height:100vh;object-fit:contain'></body>")

    def __init__(self, port=8090, quality=80):
        import cv2
        self.cv2, self.jpg, self.q, self.port = cv2, None, quality, port
        self.stopping = False
        outer = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a): pass

            def do_GET(self):
                if self.path.startswith("/stream"):
                    self.send_response(200)
                    self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=f")
                    self.end_headers()
                    last = None
                    try:
                        while not outer.stopping:
                            j = outer.jpg
                            if j is not None and j is not last:
                                self.wfile.write(b"--f\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n" % len(j) + j + b"\r\n")
                                last = j
                            time.sleep(0.01)
                    except (BrokenPipeError, ConnectionResetError):
                        return
                elif self.path.startswith("/frame.jpg"):
                    self.send_response(200); self.send_header("Content-Type", "image/jpeg"); self.end_headers()
                    self.wfile.write(outer.jpg or b"")
                else:
                    self.send_response(200); self.send_header("Content-Type", "text/html"); self.end_headers()
                    self.wfile.write(Mjpeg.PAGE)

        ThreadingHTTPServer.daemon_threads = True      # an open browser tab must not keep the game alive after it ends
        ThreadingHTTPServer.allow_reuse_address = True # a restart right after a run can bind the port again
        self.srv = ThreadingHTTPServer(("0.0.0.0", port), H)
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def show(self, frame):
        ok, buf = self.cv2.imencode(".jpg", frame, [self.cv2.IMWRITE_JPEG_QUALITY, self.q])
        if ok:
            self.jpg = buf.tobytes()

    def close(self):
        self.stopping = True
        self.srv.shutdown()
        self.srv.server_close()


class Framebuffer(NoDisplay):
    def __init__(self, dev="/dev/fb0"):
        import cv2
        self.cv2 = cv2
        base = "/sys/class/graphics/" + os.path.basename(dev)
        self.fw, self.fh = [int(v) for v in open(base + "/virtual_size").read().strip().split(",")]
        self.bpp = int(open(base + "/bits_per_pixel").read())
        self.stride = int(open(base + "/stride").read())
        self.f = open(dev, "r+b", buffering=0)

    def show(self, frame):
        img = self.cv2.resize(frame, (self.fw, self.fh))
        if self.bpp == 32:
            out = np.dstack([img, np.full(img.shape[:2], 255, np.uint8)])             # B G R A
        elif self.bpp == 16:
            b, g, r = img[..., 0].astype(np.uint16), img[..., 1].astype(np.uint16), img[..., 2].astype(np.uint16)
            out = (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).astype("<u2")
        else:
            raise RuntimeError("framebuffer depth %d not handled" % self.bpp)
        raw = out.tobytes()
        row = len(raw) // self.fh
        if self.stride != row:
            raw = b"".join(raw[i * row:(i + 1) * row].ljust(self.stride, b"\0") for i in range(self.fh))
        self.f.seek(0)
        self.f.write(raw)

    def close(self):
        self.f.close()


class GstKms(NoDisplay):
    def __init__(self, w, h, fps=30):
        import gi
        gi.require_version("Gst", "1.0")
        from gi.repository import Gst
        Gst.init(None)
        self.Gst = Gst
        desc = ("appsrc name=src is-live=true format=time block=false caps=video/x-raw,format=BGR,width=%d,height=%d,framerate=%d/1 "
                "! videoconvert ! video/x-raw,format=BGRx ! kmssink driver-name=tidss sync=false force-modesetting=true" % (w, h, fps))
        self.pipe = Gst.parse_launch(desc)
        self.src = self.pipe.get_by_name("src")
        self.pipe.set_state(Gst.State.PLAYING)

    def show(self, frame):
        self.src.emit("push-buffer", self.Gst.Buffer.new_wrapped(frame.tobytes()))

    def close(self):
        self.pipe.set_state(self.Gst.State.NULL)


class VideoFile(NoDisplay):
    def __init__(self, path, w, h, fps=30):
        import cv2
        self.out = cv2.VideoWriter(path, cv2.VideoWriter_fourcc(*"MJPG"), fps, (w, h))

    def show(self, frame):
        self.out.write(frame)

    def close(self):
        self.out.release()


def make(kind, w, h, arg=None):
    if kind == "window": return Window(w, h)
    if kind == "mjpeg": return Mjpeg(int(arg or 8090))
    if kind == "fb": return Framebuffer(arg or "/dev/fb0")
    if kind == "gst": return GstKms(w, h)
    if kind == "file": return VideoFile(arg or "bubbles.avi", w, h)
    return NoDisplay()

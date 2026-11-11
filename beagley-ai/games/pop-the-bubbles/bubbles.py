"""Pop the bubbles: game rules and drawing. No camera, no model and no sound in here, so it can be tested on its own.

Made for a 5-year-old: big slow bubbles, a big touch zone, a hand sweeping through a bubble pops it even between two camera frames,
nothing can be lost, no timer, no game over. Score is a number and a row of stars. Every ten pops a small party.
All art is drawn from circles, lines and stars. Positions are in canvas pixels; hands arrive as 0..1 and are scaled here.
"""
import math
import random

import numpy as np

PALETTE = [(180, 140, 255), (255, 190, 120), (170, 235, 140), (120, 220, 255), (230, 150, 230), (150, 170, 255)]  # BGR pastels
HAND_R = 46          # drawn hand ring, canvas pixels at 1280x720
TOUCH_PAD = 34       # extra reach added to a bubble's radius when a hand touches it
TARGET_BUBBLES = 5
PARTY_EVERY = 10


class Bubble:
    __slots__ = ("x", "y", "r", "vy", "phase", "wob", "color")

    def __init__(self, x, y, r, vy, phase, wob, color):
        self.x, self.y, self.r, self.vy, self.phase, self.wob, self.color = x, y, r, vy, phase, wob, color


class Hand:
    __slots__ = ("id", "x", "y", "px", "py", "seen")

    def __init__(self, id, x, y, t):
        self.id, self.x, self.y, self.px, self.py, self.seen = id, x, y, x, y, t


def seg_circle_hit(ax, ay, bx, by, cx, cy, rad):
    """True when the segment A-B comes within rad of C: a fast sweep pops what it passes through."""
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((cx - ax) * dx + (cy - ay) * dy) / L2))
    px, py = ax + t * dx, ay + t * dy
    return (px - cx) ** 2 + (py - cy) ** 2 <= rad * rad


class BubbleGame:
    def __init__(self, w=1280, h=720, seed=None, target=TARGET_BUBBLES):
        self.w, self.h = w, h
        self.k = w / 1280.0
        self.rng = random.Random(seed)
        self.target = target
        self.bubbles = []
        self.hands = {}
        self.particles = []   # [x, y, vx, vy, life, color, size]
        self.rings = []       # [x, y, r, life, color]
        self.score = 0
        self.party = 0.0
        self.last_hand_t = None
        self.t = 0.0
        self.events = []      # ("pop", note_index) or ("party", 0), drained by the caller for sound
        for _ in range(target):
            self._spawn(initial=True)

    # ---- rules -------------------------------------------------------------------------------------------------------------
    def _spawn(self, initial=False):
        r = self.rng.uniform(58, 98) * self.k
        x = self.rng.uniform(r, self.w - r)
        y = self.rng.uniform(self.h * 0.25, self.h * 0.95) if initial else self.h + r + self.rng.uniform(0, 120) * self.k
        self.bubbles.append(Bubble(x, y, r, self.rng.uniform(55, 100) * self.k, self.rng.uniform(0, 6.28), self.rng.uniform(10, 26) * self.k,
                                   self.rng.choice(PALETTE)))

    def set_hands(self, hands, now):
        """hands: list of {"id", "x", "y"} in 0..1. Keeps a short memory per id so the ring glides and a sweep can be tested."""
        seen = set()
        for h in hands:
            hid = h["id"]
            x, y = h["x"] * self.w, h["y"] * self.h
            seen.add(hid)
            if hid in self.hands:
                o = self.hands[hid]
                o.px, o.py = o.x, o.y
                o.x, o.y = o.x + 0.65 * (x - o.x), o.y + 0.65 * (y - o.y)   # light smoothing against jitter
                o.seen = now
            else:
                self.hands[hid] = Hand(hid, x, y, now)
        for hid in [i for i, o in self.hands.items() if now - o.seen > 0.5]:
            del self.hands[hid]
        if hands:
            self.last_hand_t = now

    def update(self, dt, now=None):
        self.t += dt
        now = self.t if now is None else now
        reach = TOUCH_PAD * self.k + HAND_R * self.k * 0.5
        popped = []
        for b in self.bubbles:
            b.y -= b.vy * dt
            b.phase += dt * 1.6
            bx = b.x + math.sin(b.phase) * b.wob
            for h in self.hands.values():
                if seg_circle_hit(h.px, h.py, h.x, h.y, bx, b.y, b.r + reach):
                    popped.append((b, bx))
                    break
        for b in self.bubbles:                      # a bubble that floats off the top comes back from the bottom: nothing is lost
            if b.y < -b.r * 1.5:
                b.y = self.h + b.r + self.rng.uniform(0, 80) * self.k
                b.x = self.rng.uniform(b.r, self.w - b.r)
        before = self.score // PARTY_EVERY
        for b, bx in popped:
            self.bubbles.remove(b)
            self.score += 1
            self._burst(bx, b.y, b.color, b.r)
            self.events.append(("pop", (self.score - 1) % 5))
        if self.score // PARTY_EVERY > before:       # two pops in one tick can step over a multiple of ten
            self.party = 2.5
            self.events.append(("party", 0))
            for _ in range(60):
                self.particles.append([self.rng.uniform(0, self.w), -10, self.rng.uniform(-60, 60) * self.k, self.rng.uniform(150, 320) * self.k,
                                       2.8, self.rng.choice(PALETTE), self.rng.uniform(7, 13) * self.k])
        while len(self.bubbles) < self.target:
            self._spawn()
        for p in self.particles:
            p[0] += p[2] * dt
            p[1] += p[3] * dt
            p[3] += 260 * self.k * dt * (1 if p[6] > 6 and p[4] > 0 else 0.5)   # gentle fall
            p[4] -= dt
        self.particles = [p for p in self.particles if p[4] > 0]
        for r in self.rings:
            r[3] -= dt
        self.rings = [r for r in self.rings if r[3] > 0]
        self.party = max(0.0, self.party - dt)
        # hands that did not move this tick stop sweeping
        for h in self.hands.values():
            h.px, h.py = h.x, h.y

    def _burst(self, x, y, color, r):
        for i in range(16):
            a = self.rng.uniform(0, 6.28)
            sp = self.rng.uniform(120, 360) * self.k
            self.particles.append([x, y, math.cos(a) * sp, math.sin(a) * sp, self.rng.uniform(0.5, 0.9), color, self.rng.uniform(5, 10) * self.k])
        self.rings.append([x, y, r, 0.45, color])

    def drain_events(self):
        e, self.events = self.events, []
        return e

    # ---- drawing -----------------------------------------------------------------------------------------------------------
    def render(self, camera=None, people=None, idle_hint=True, debug=None):
        import cv2
        w, h, k = self.w, self.h, self.k
        # Speed (BeagleY-AI, 2026-11-04): the gradient took 43 ms and a mirrored resize of the camera 58 ms on every
        # frame. The gradient is made once; the camera background is remade only when a new camera frame arrives
        # (pose runs about 13 a second, drawing wants 30), resizing first and mirroring after (18.6 ms).
        if getattr(self, "_sky", None) is None or self._sky.shape[:2] != (h, w):
            self._sky = np.empty((h, w, 3), np.uint8)
            self._sky[:] = np.linspace((255, 235, 200), (255, 215, 245), h, dtype=np.float32)[:, None, :].astype(np.uint8)   # pale blue to pale pink
            self._bg_src, self._bg = None, self._sky
        if camera is None:
            self._bg_src, self._bg = None, self._sky
        elif camera is not self._bg_src:
            cam = cv2.flip(cv2.resize(camera, (w, h)), 1)                  # mirrored, so the screen is a mirror
            self._bg, self._bg_src = cv2.addWeighted(cam, 0.42, self._sky, 0.58, 0), camera
        frame = self._bg.copy()
        for b in self.bubbles:                                             # translucent fill, blended inside each bubble's box only
            cx, cy, r = int(b.x + math.sin(b.phase) * b.wob), int(b.y), int(b.r)
            x0, y0, x1, y1 = max(0, cx - r - 2), max(0, cy - r - 2), min(w, cx + r + 3), min(h, cy + r + 3)
            if x1 <= x0 or y1 <= y0:
                continue
            roi = frame[y0:y1, x0:x1]
            layer = roi.copy()
            cv2.circle(layer, (cx - x0, cy - y0), r, b.color, -1, cv2.LINE_AA)
            cv2.addWeighted(layer, 0.42, roi, 0.58, 0, roi)
        for b in self.bubbles:
            cx, cy = int(b.x + math.sin(b.phase) * b.wob), int(b.y)
            cv2.circle(frame, (cx, cy), int(b.r), b.color, max(3, int(5 * k)), cv2.LINE_AA)
            cv2.ellipse(frame, (cx, cy), (int(b.r * 0.72), int(b.r * 0.72)), 0, 200, 260, (255, 255, 255), max(3, int(6 * k)), cv2.LINE_AA)   # shine
            cv2.circle(frame, (int(cx - b.r * 0.38), int(cy - b.r * 0.46)), max(3, int(b.r * 0.09)), (255, 255, 255), -1, cv2.LINE_AA)
        for r in self.rings:
            f = 1 - r[3] / 0.45
            cv2.circle(frame, (int(r[0]), int(r[1])), int(r[2] * (1 + 0.5 * f)), r[4], max(2, int(5 * k * (1 - f))), cv2.LINE_AA)
        for p in self.particles:
            self._star(frame, p[0], p[1], p[6], p[5], p[4])
        if debug and people:
            for pe in people:
                self._debug_person(frame, pe, debug)
        for hd in self.hands.values():                                      # the player's hands: warm ring with a white edge
            c = (int(hd.x), int(hd.y))
            cv2.circle(frame, c, int(HAND_R * k) + 5, (255, 255, 255), -1, cv2.LINE_AA)
            cv2.circle(frame, c, int(HAND_R * k), (60, 200, 255), -1, cv2.LINE_AA)
            cv2.circle(frame, c, int(HAND_R * k * 0.45), (255, 255, 255), -1, cv2.LINE_AA)
        self._score_badge(frame)
        if idle_hint and (self.last_hand_t is None or self.t - self.last_hand_t > 5.0 and not self.hands):
            self._hint(frame)
        if self.party > 0:
            self._banner(frame)
        return frame

    def _star(self, img, x, y, size, color, life=1.0, fill=True):
        import cv2
        pts = []
        for i in range(10):
            rr = size if i % 2 == 0 else size * 0.45
            a = -math.pi / 2 + i * math.pi / 5
            pts.append((int(x + rr * math.cos(a)), int(y + rr * math.sin(a))))
        pts = np.array(pts, np.int32)
        cv2.fillPoly(img, [pts], color, cv2.LINE_AA) if fill else cv2.polylines(img, [pts], True, color, 3, cv2.LINE_AA)

    def _score_badge(self, frame):
        import cv2
        k = self.k
        x0, y0, bw, bh = int(24 * k), int(20 * k), int(300 * k), int(96 * k)
        roi = frame[y0:y0 + bh + 1, x0:x0 + bw + 1]                       # white panel at 75 percent, blended in its box only
        cv2.addWeighted(roi, 0.25, np.full_like(roi, 255), 0.75, 0, roi)
        self._star(frame, x0 + 50 * k, y0 + bh / 2, 34 * k, (60, 210, 255))
        cv2.putText(frame, str(self.score), (int(x0 + 105 * k), int(y0 + bh / 2 + 28 * k)), cv2.FONT_HERSHEY_DUPLEX, 2.4 * k, (90, 60, 40), max(3, int(5 * k)), cv2.LINE_AA)
        n = self.score % PARTY_EVERY
        for i in range(PARTY_EVERY):     # ten dots to the next party
            c = (int(x0 + (24 + i * 27) * k), int(y0 + bh + 18 * k))
            cv2.circle(frame, c, int(9 * k), (60, 210, 255) if i < n else (235, 225, 215), -1, cv2.LINE_AA)

    def _hint(self, frame):
        import cv2
        k = self.k
        pulse = 0.5 + 0.5 * math.sin(self.t * 3)
        cx, cy = self.w // 2, int(self.h * 0.5)
        cv2.circle(frame, (cx, cy), int((90 + 18 * pulse) * k), (60, 200, 255), int(8 * k), cv2.LINE_AA)
        txt = "Wave your hands!"
        (tw, _), _ = cv2.getTextSize(txt, cv2.FONT_HERSHEY_DUPLEX, 2.0 * k, int(5 * k))
        cv2.putText(frame, txt, (cx - tw // 2, int(cy + 190 * k)), cv2.FONT_HERSHEY_DUPLEX, 2.0 * k, (255, 255, 255), int(11 * k), cv2.LINE_AA)
        cv2.putText(frame, txt, (cx - tw // 2, int(cy + 190 * k)), cv2.FONT_HERSHEY_DUPLEX, 2.0 * k, (90, 60, 40), int(4 * k), cv2.LINE_AA)

    def _banner(self, frame):
        import cv2
        k = self.k
        bounce = 1 + 0.08 * math.sin(self.t * 12)
        for i, c in enumerate(PALETTE):
            self._star(frame, self.w * (0.2 + 0.12 * i), self.h * 0.5 + 20 * math.sin(self.t * 6 + i), 48 * k * bounce, c)
        txt = "%d!" % self.score
        sc = 5.0 * k * bounce
        (tw, th), _ = cv2.getTextSize(txt, cv2.FONT_HERSHEY_DUPLEX, sc, int(10 * k))
        org = (self.w // 2 - tw // 2, int(self.h * 0.5 + th / 2) - int(90 * k))
        cv2.putText(frame, txt, org, cv2.FONT_HERSHEY_DUPLEX, sc, (255, 255, 255), int(26 * k), cv2.LINE_AA)
        cv2.putText(frame, txt, org, cv2.FONT_HERSHEY_DUPLEX, sc, (60, 150, 255), int(10 * k), cv2.LINE_AA)

    def _debug_person(self, frame, pe, cam_wh):
        import cv2
        fw, fh = cam_wh
        kp = pe["kp"]
        pairs = [(5, 7), (7, 9), (6, 8), (8, 10), (5, 6), (5, 11), (6, 12), (11, 12)]
        def pt(i):
            return int((1 - kp[i][0] / fw) * self.w), int(kp[i][1] / fh * self.h)
        for a, b in pairs:
            if kp[a][2] > 0.3 and kp[b][2] > 0.3:
                cv2.line(frame, pt(a), pt(b), (255, 255, 255), 3, cv2.LINE_AA)

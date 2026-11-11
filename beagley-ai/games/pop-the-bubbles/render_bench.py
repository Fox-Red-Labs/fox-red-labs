"""Time BubbleGame.render alone: python3 render_bench.py <game dir> [w h]. Camera frame = a fixed 1280x768 image that changes every 3rd call (pose runs ~13/s, draw wants 30)."""
import sys, time, numpy as np
sys.path.insert(0, sys.argv[1]); import bubbles
w, h = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (1280, 720)
g = bubbles.BubbleGame(w, h, seed=1)
rng = np.random.default_rng(0)
cams = [rng.integers(0, 255, (768, 1280, 3), dtype=np.uint8) for _ in range(4)]
g.score = 7
for _ in range(10): g.update(1/30); g.render(cams[0])
ts = []
for i in range(150):
    g.update(1/30)
    t = time.perf_counter(); g.render(cams[(i // 3) % 4]); ts.append((time.perf_counter() - t) * 1000)
ts.sort(); print("render_ms median %.2f p90 %.2f  (%d bubbles)" % (ts[len(ts)//2], ts[int(len(ts)*0.9)], len(g.bubbles)))

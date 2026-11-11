#!/usr/bin/env python3
"""Pop the bubbles: a camera game for small children. Body pose from a webcam; touch the bubbles with your hands.

  Mac, development (CPU model):      python3 play.py --pose cpu --cam 0
  Mac, from a recorded clip:         python3 play.py --pose cpu --cam clip.mp4
  Mac, mouse as the hand:            python3 play.py --pose mouse
  BeagleY-AI, accelerated pose:      python3 play.py --pose tidl --cam /dev/video-usb-cam0 --display fb
  Any browser as the screen:         python3 play.py --pose tidl --display mjpeg        then open http://<address>:8090/
Keys in the window: q or Esc quit, m mute, d show the skeleton and the speeds.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bubbles
import display
import pose
import sound


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pose", default="auto", choices=["auto", "cpu", "tidl", "mouse", "demo"])
    ap.add_argument("--cam", default="0", help="camera index, /dev/video device, or a video file")
    ap.add_argument("--model", default=None, help="folder with model/*.onnx and artifacts/ (default: search)")
    ap.add_argument("--display", default="window", choices=["window", "mjpeg", "fb", "gst", "file", "none"])
    ap.add_argument("--display-arg", default=None, help="mjpeg port, fb device or output file")
    ap.add_argument("--size", default=None, help="default 1280x720; 960x540 for --display mjpeg (JPEG encoding is the limit there)")
    ap.add_argument("--fps", type=int, default=30)
    ap.add_argument("--mute", action="store_true")
    ap.add_argument("--debug", action="store_true")
    ap.add_argument("--seconds", type=float, default=0, help="stop after this long (tests, timing runs)")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--log", default=None, help="write one line a second (fps, model ms, score) to this file")
    a = ap.parse_args(argv)
    size = a.size or ("960x540" if a.display == "mjpeg" else "1280x720")
    w, h = [int(v) for v in size.split("x")]

    kind = a.pose
    if kind == "auto":
        kind = "tidl" if os.path.isdir("/opt/model_zoo/" + pose.MODEL_NAME) else "cpu"
    worker = pose.PoseWorker(kind, a.cam, a.model)
    worker.start()
    game = bubbles.BubbleGame(w, h, seed=a.seed)
    snd = sound.Sound(a.mute)
    disp = display.make(a.display, w, h, a.display_arg)
    print("pose=%s display=%s sound=%s size=%dx%d" % (kind, a.display, snd.describe(), w, h), flush=True)

    t0 = last = time.time()
    seq_seen, shown, tlog = -1, 0, t0
    debug = a.debug
    logf = open(a.log, "w") if a.log else None
    try:
        while True:
            now = time.time()
            dt = min(0.1, now - last)
            last = now
            lat = worker.get()
            if kind == "mouse" and disp.mouse:
                worker.mouse = (disp.mouse[0] / w, disp.mouse[1] / h)
            if lat["seq"] != seq_seen:
                seq_seen = lat["seq"]
                game.set_hands(lat["hands"], game.t)
            game.update(dt)
            for ev in game.drain_events():
                snd.play(ev)
            cam = lat["frame"]
            img = game.render(cam, lat["people"] if debug else None, debug=(cam.shape[1], cam.shape[0]) if (debug and cam is not None) else None)
            if debug:
                import cv2
                cv2.putText(img, "pose %.1f fps  model %.0f ms" % (worker.fps, worker.infer_ms), (20, h - 20), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (60, 60, 60), 2, cv2.LINE_AA)
            disp.show(img)
            shown += 1
            k = disp.poll_key()
            if k in (ord("q"), 27):
                break
            if k == ord("m"):
                snd.mute = not snd.mute
            if k == ord("d"):
                debug = not debug
            if logf and now - tlog >= 1.0:
                logf.write("%.1f draw_fps=%.1f pose_fps=%.1f model_ms=%.1f score=%d hands=%d\n" % (now - t0, shown / (now - tlog), worker.fps, worker.infer_ms, game.score, len(game.hands)))
                logf.flush()
                shown, tlog = 0, now
            if a.seconds and now - t0 >= a.seconds:
                break
            sl = 1.0 / a.fps - (time.time() - now)
            if sl > 0:
                time.sleep(sl)
    except KeyboardInterrupt:
        pass
    finally:
        worker.stop()
        worker.join(timeout=2)
        disp.close()
    print("score", game.score)
    return game.score


if __name__ == "__main__":
    main()

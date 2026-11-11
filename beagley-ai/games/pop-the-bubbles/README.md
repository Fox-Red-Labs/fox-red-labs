# Pop the bubbles

Staged 2026-11-01; ran on the BeagleY-AI on 2026-11-04 from a recorded clip: pose on the C7x 13 times a second, and after a drawing speed-up the game redraws 20 frames a second at 1280x720 and 23 in a browser at 960x540 (`speedup-2026-11-04.log`; before it, 5 to 6: `board-run-2026-11-04.log`). `render_bench.py` times the drawing alone. Runs on a Mac with a webcam, a recorded clip, or the mouse. Run with the board's USB camera on 2026-11-05 for 20 s with no display: 29.8 draw fps, pose 11.0 a second, model 57 ms (`../../bench/camera-power/p63-cam20.log`); with the browser display the board rebooted within 25 s (cause unknown). Not yet run with HDMI or sound.

Big slow bubbles float up. Touch them with your hands to pop them: a happy note, a spray of stars, a score with a star. Every tenth pop is a small party. Nothing can be lost: no timer, no game over, bubbles that float off the top come back from the bottom. A hand sweeping through a bubble between two camera frames still pops it. All art is circles, lines and stars drawn in code; the sounds are sine waves made at start-up.

## Run it (Mac, development)

```bash
python3 -m venv ~/bubbles-venv && ~/bubbles-venv/bin/pip install numpy opencv-python onnxruntime
# the pose model: ONR-KD-7060 for AM67A (46.6 MB), unpack it and point POSE_MODEL at the folder that holds model/ and artifacts/
curl -fLO https://downloads.ti.com/jacinto7/esd/modelzoo/11_00_00/modelartifacts/AM67A/8bits/kd-7060_onnxrt_coco_edgeai-yolox_yolox_s_pose_ti_lite_640_20220301_model_onnx.tar.gz
mkdir ONR-KD-7060 && tar -xzf kd-7060*.tar.gz -C ONR-KD-7060 && export POSE_MODEL=$PWD/ONR-KD-7060
cd beagley-ai/games/pop-the-bubbles
python3 play.py --pose cpu --cam 0                 # webcam
python3 play.py --pose cpu --cam clip.mp4          # a recorded clip, played at its own speed in a loop
python3 play.py --pose mouse                       # the mouse is a hand
python3 play.py --pose demo --display none --seconds 10   # no input, no window: a timing run
```

Keys: q or Esc quit, m mute, d show the skeleton and the model speed.

## Run it (BeagleY-AI, after the arrival plan)

```bash
python3 play.py --pose tidl --cam /dev/video-usb-cam0 --display mjpeg   # open http://<board>:8090/ in a browser
python3 play.py --pose tidl --cam /dev/video-usb-cam0 --display fb      # HDMI through the framebuffer (stop edgeai-gui-app first)
python3 play.py --pose tidl --cam /dev/video-usb-cam0 --display gst     # HDMI through GStreamer kmssink
```

`--pose tidl` runs the same model on the C7x through onnxruntime's TIDL provider with the artifacts folder in `/opt/model_zoo/ONR-KD-7060-human-pose-yolox-s-640x640`. `--pose cpu` is only for the Mac (seconds per frame on the board's A53s).

## Checked and not checked

Checked on a Mac, 2026-11-01: the model's output format (57 values per person: box, score, class, 17 keypoints of x, y, confidence in 640x640 pixels, bottom-right padded); the CPU output has no non-maximum suppression and raw confidence logits, which `pose.decode` fixes; rules, scoring, party, sweep hits and no-loss behavior (`tools/tests/test_pop_the_bubbles.py`); a 25 s run on a recorded clip.

Checked on the board, 2026-11-04: the TIDL provider path (274 of 274 nodes offloaded), `--display file` and `--display mjpeg`. Not checked (needs the board's camera, HDMI or speaker): `cv2.VideoCapture` on `/dev/video-usb-cam0` with the board's OpenCV (the code falls back to a GStreamer pipeline), the three HDMI paths, the sound player on the board (`aplay`, `paplay` or GStreamer, first found), the frame rate on four A53 cores (use `--size 960x540` if low). The webcam's mirror image is drawn behind the bubbles so the child sees themself; nothing is saved.

import sys, time, json, threading, subprocess, serial, radar_live as r
cfg, out = sys.argv[1], sys.argv[2]
cues = [(0,"Radar test starting. Please stand up and walk slowly away from the radar, to the far side of the room."),
        (15,"Now walk back toward the radar, and stop about one meter in front of it."),
        (28,"Walk side to side, left, then right."),
        (40,"Sit down and stay still."),
        (55,"Now wave one arm."),
        (65,"Leave the radar's view, or step behind it."),
        (78,"Test done. Thank you.")]
bad = r.configure('/dev/cu.usbmodemRI321', cfg, 'RI32'); print('config errors', bad, flush=True)
s = serial.Serial('/dev/cu.usbmodemRI321', 115200, timeout=0.05)
log = []; t0 = time.time(); buf = b''; raw = open(out + '.bin', 'wb')
def speak():
    for t, msg in cues:
        while time.time() - t0 < t: time.sleep(0.05)
        log.append({'t': round(time.time()-t0, 2), 'cue': msg}); subprocess.run(['say', msg])
th = threading.Thread(target=speak); th.start()
frames = []
while time.time() - t0 < 82:
    c = s.read(4096); raw.write(c)
    fr, buf = r.split_frames(buf + c)
    for f in fr:
        d = r.decode(f); d['t'] = round(time.time()-t0, 2); frames.append(d)
th.join()
json.dump({'cues': log, 'frames': frames}, open(out + '.json', 'w'))
print('frames', len(frames), 'with tracks', sum(1 for f in frames if f['tracks']))

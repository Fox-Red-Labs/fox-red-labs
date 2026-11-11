"""Happy sounds, made from sine waves at start-up (no sound files, nothing copied from anywhere).

Five pop notes climb a pentatonic scale so any order of pops sounds nice; a short fanfare plays at every tenth pop.
Playback runs a small player program: afplay on a Mac; aplay, paplay or a GStreamer one-liner on the board (first one found).
"""
import os
import shutil
import subprocess
import tempfile
import wave

import numpy as np

SR = 22050
NOTES = [523.25, 587.33, 659.25, 783.99, 880.0]   # C5 D5 E5 G5 A5


def _env(n, attack=0.004, decay=6.0):
    t = np.arange(n) / SR
    return np.minimum(1, t / attack) * np.exp(-decay * t)


def pop(freq):
    n = int(SR * 0.28)
    t = np.arange(n) / SR
    sweep = np.sin(2 * np.pi * (freq * 0.6 * t + freq * 0.9 * (1 - np.exp(-t * 30)) / 30))   # quick rise: a bloop
    shimmer = 0.25 * np.sin(2 * np.pi * freq * 2 * t)
    click = np.random.RandomState(1).randn(n) * np.exp(-t * 180) * 0.35
    return (sweep + shimmer) * _env(n, decay=9.0) * 0.6 + click * 0.5


def fanfare():
    out = np.zeros(int(SR * 1.1))
    for i, f in enumerate([523.25, 659.25, 783.99, 1046.5]):
        s = int(SR * 0.12 * i)
        n = int(SR * 0.5)
        t = np.arange(n) / SR
        out[s:s + n] += (np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * f * 2 * t)) * _env(n, decay=4.5) * 0.35
    return out


def write_wav(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32000).astype("<i2").tobytes())


class Sound:
    def __init__(self, mute=False):
        self.dir = tempfile.mkdtemp(prefix="bubbles-snd-")
        self.files = {}
        for i, f in enumerate(NOTES):
            self.files[("pop", i)] = os.path.join(self.dir, "pop%d.wav" % i)
            write_wav(self.files[("pop", i)], pop(f))
        self.files[("party", 0)] = os.path.join(self.dir, "party.wav")
        write_wav(self.files[("party", 0)], fanfare())
        self.mute = mute
        self.player = None if mute else self._find_player()
        self.procs = []

    @staticmethod
    def _find_player():
        for cmd in (["afplay"], ["aplay", "-q"], ["paplay"]):
            if shutil.which(cmd[0]):
                return cmd
        if shutil.which("gst-launch-1.0"):
            return ["gst-launch-1.0", "-q", "__GST__"]
        return None

    def play(self, event):
        if self.mute or not self.player or event not in self.files:
            return
        self.procs = [p for p in self.procs if p.poll() is None]
        if len(self.procs) >= 4:
            return
        path = self.files[event]
        if self.player[-1] == "__GST__":
            cmd = ["gst-launch-1.0", "-q", "filesrc", "location=" + path, "!", "wavparse", "!", "audioconvert", "!", "audioresample", "!", "autoaudiosink"]
        else:
            cmd = self.player + [path]
        try:
            self.procs.append(subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        except OSError:
            self.player = None

    def describe(self):
        return "muted" if self.mute else (" ".join(self.player) if self.player else "no player found (silent)")

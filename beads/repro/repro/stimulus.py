# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# -----------------------------------------------------------------------------


"""Stimulus builders: the same arrays on every tier, from a scenario and a seed."""

from dataclasses import dataclass
import hashlib
import struct

import numpy as np

from repro.scenario import CV_CHANNELS

MAGIC = 0x4F525052  # "RPRO"
BLOCK = 12  # frames per processor block; the stimulus length is a multiple


@dataclass
class Stimulus:
    sr: int
    audio: np.ndarray    # (n, 2) float32
    cv: dict             # channel -> (n,) float32, firmware units
    gates: dict          # "seed"/"freeze" -> (n,) uint8

    @property
    def frames(self):
        return len(self.audio)

    def pack(self):
        """Driver input: header, audio, six CV tracks, seed gate, freeze gate."""
        parts = [struct.pack("<II", MAGIC, self.frames),
                 np.ascontiguousarray(self.audio, "<f4").tobytes()]
        parts += [np.ascontiguousarray(self.cv[c], "<f4").tobytes()
                  for c in CV_CHANNELS]
        parts += [self.gates["seed"].tobytes(), self.gates["freeze"].tobytes()]
        return b"".join(parts)

    def sha256(self):
        return hashlib.sha256(self.pack()).hexdigest()


def frames_of(seconds, sr):
    return int(round(seconds * sr))


def _sine(n, sr, freq, amp):
    t = np.arange(n) / sr
    return amp * np.sin(2 * np.pi * freq * t)


def _chirps(n, sr, f0, f1, amp, period_s):
    """Rising chirps f0 -> f1 every period_s, so a grain's slope sign is its
    playback direction."""
    period = frames_of(period_s, sr)
    k = np.arange(n) % period
    t = k / sr
    ph = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * period_s))
    fade = max(1, frames_of(0.01, sr))
    w = np.minimum(1.0, np.minimum(k, period - k) / fade)
    return amp * w * np.sin(ph)


def _cv_track(n, sr, events, scale):
    v = np.zeros(n)
    for ev in sorted(events, key=lambda e: e["t"]):
        s = frames_of(ev["t"], sr)
        if s >= n:
            continue
        cur = v[s - 1] if s > 0 else 0.0
        target = ev["level"] * scale
        ramp = frames_of(ev.get("ramp", 0.0), sr)
        if ramp > 0:
            m = min(ramp, n - s)
            v[s:s + m] = cur + (target - cur) * np.arange(1, m + 1) / ramp
            v[s + m:] = target
        else:
            v[s:] = target
    return v.astype(np.float32)


def _gate_track(n, sr, events):
    g = np.zeros(n, np.uint8)
    for ev in events:
        if ev["kind"] == "trigger":
            width = ev.get("width", 0.005)
            for t in ev["times"]:
                s = frames_of(t, sr)
                g[s:s + max(1, frames_of(width, sr))] = 1
        else:
            g[frames_of(ev["t0"], sr):frames_of(ev["t1"], sr)] = 1
    return g


def build(scenario, sr, seed):
    """Stimulus for `scenario` at `sr`; `seed` only feeds optional noise."""
    n = frames_of(scenario.seconds, sr) // BLOCK * BLOCK
    gen = scenario.generator
    kind = gen["generator"]
    if kind == "sine":
        mono = _sine(n, sr, gen["freq"], gen["amp"])
    elif kind == "chirps":
        mono = _chirps(n, sr, gen["f0"], gen["f1"], gen["amp"], gen["period_s"])
    else:
        mono = np.zeros(n)
    noise = gen.get("noise", 0.0)
    if noise:
        mono = mono + noise * np.random.Generator(
            np.random.PCG64(seed)).standard_normal(n)
    audio = np.stack([mono, mono], 1).astype(np.float32)
    cv = {c: _cv_track(n, sr, [e for e in scenario.events
                               if e["kind"] == "cv" and e["channel"] == c],
                       scenario.analog[c])
          for c in CV_CHANNELS}
    gates = {g: _gate_track(n, sr, [e for e in scenario.events
                                    if e["kind"] != "cv" and e["channel"] == g])
             for g in ("seed", "freeze")}
    return Stimulus(sr, audio, cv, gates)

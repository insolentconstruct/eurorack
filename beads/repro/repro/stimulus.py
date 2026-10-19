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

from repro.scenario import CV_CHANNELS, series

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
            for t in series(ev["times"]):
                s = frames_of(t, sr)
                g[s:s + max(1, frames_of(width, sr))] = 1
        else:
            g[frames_of(ev["t0"], sr):frames_of(ev["t1"], sr)] = 1
    return g


# ---------------------------------------------------------------------------
# Primitive builders. Frame arguments are in samples at `sr`; the layer
# renderer below converts a scenario's seconds.
# ---------------------------------------------------------------------------
def sine(n, sr, f, amp, start=0, stop=None, fade_s=0.01, phase_s=0.0):
    """A sine from frame `start` to `stop`, with linear fades at both ends.
    `phase_s` advances the oscillator, as if it had started that many seconds
    before frame `start` (the steady state of a tone begun earlier)."""
    out = np.zeros(n)
    stop = n if stop is None else min(n, stop)
    k = np.arange(stop - start)
    w = np.ones(len(k))
    fd = min(frames_of(fade_s, sr), len(k) // 2)
    if fd:
        w[:fd] = np.linspace(0, 1, fd)
        w[-fd:] = np.linspace(1, 0, fd)
    out[start:stop] = amp * w * np.sin(
        2 * np.pi * f * k / sr + 2 * np.pi * f * phase_s)
    return out


def steps(n, sr, seg_s, freqs, amp, start_s=0.0):
    """Back-to-back sines of `seg_s` seconds cycling through `freqs`, so a
    tone's frequency says when it was played."""
    out = np.zeros(n)
    seg = frames_of(seg_s, sr)
    if seg < 1:
        raise ValueError(f"steps: seg_s {seg_s} is less than one frame")
    i, j = frames_of(start_s, sr), 0
    while i < n:
        out += sine(n, sr, freqs[j % len(freqs)], amp, i, i + seg)
        i += seg
        j += 1
    return out


def chirp(n, sr, f0, f1, amp, start, dur_s):
    """One rising chirp of `dur_s` seconds from frame `start`."""
    out = np.zeros(n)
    k = np.arange(min(frames_of(dur_s, sr), n - start))
    t = k / sr
    ph = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * dur_s))
    w = np.minimum(1, np.minimum(k, len(k) - k) / frames_of(0.01, sr))
    out[start:start + len(k)] = amp * w * np.sin(ph)
    return out


def tones(spec):
    """A list of frequencies, written out or as {start, per_octave, count}."""
    if isinstance(spec, dict):
        return [spec["start"] * 2 ** (k / spec["per_octave"])
                for k in range(spec["count"])]
    return list(spec)


def _repeats(layer):
    """Start times of a layer: one, or `count` of them every `every` s."""
    first = layer.get("start", 0.0)
    return [first + layer["every"] * i
            for i in range(layer.get("count", 1))] if "every" in layer \
        else [first]


def render_layers(layers, n, sr, seed):
    """Sum of the layers, (n, 2). Kinds: sine (optional `every`/`count`
    repeats, `stop`, `fade_s`, `phase_s`), steps (`seg_s`, `freqs`, optional `stop`),
    chirp (one or repeated every `every` s, `dur_s`), cyclic_chirp (a chirp
    that restarts every `period_s`, whose clock starts at `phase_s`), noise
    (uniform, `amp`, `seed` or the scenario seed, `stop`, `fade_s`, `stereo`).
    A layer plays on `channel` 0, 1 or both (the default)."""
    out = np.zeros((n, 2))
    for layer in layers:
        kind = layer["kind"]
        amp = layer["amp"]
        if kind == "sine":
            fade = layer.get("fade_s", 0.01)
            sig = np.zeros(n)
            for t0 in _repeats(layer):
                stop = layer.get("stop")
                if stop is None and "dur_s" in layer:
                    stop = t0 + layer["dur_s"]
                sig += sine(n, sr, layer["freq"], amp, frames_of(t0, sr),
                            None if stop is None else frames_of(stop, sr), fade,
                            layer.get("phase_s", 0.0))
        elif kind == "steps":
            sig = steps(n, sr, layer["seg_s"], tones(layer["freqs"]), amp,
                        layer.get("start", 0.0))
            if "stop" in layer:
                sig[frames_of(layer["stop"], sr):] = 0
        elif kind == "chirp":
            sig = np.zeros(n)
            for t0 in _repeats(layer):
                sig += chirp(n, sr, layer["f0"], layer["f1"], amp,
                             frames_of(t0, sr), layer["dur_s"])
        elif kind == "cyclic_chirp":
            clock = (np.arange(n) / sr + layer.get("phase_s", 0.0)) \
                % layer["period_s"]
            sig = amp * np.sin(2 * np.pi * (
                layer["f0"] * clock
                + 0.5 * layer["rate_hz_per_s"] * clock ** 2))
        elif kind == "noise":
            b = min(frames_of(layer["stop"], sr), n) if "stop" in layer else n
            a = min(frames_of(layer.get("start", 0.0), sr), b)
            shape = (b - a, 2) if layer.get("stereo") else b - a
            rng = np.random.Generator(np.random.PCG64(layer.get("seed", seed)))
            fill = rng.uniform(-1.0, 1.0, shape)
            fade = frames_of(layer.get("fade_s", 0.0), sr)
            if fade:
                idx = np.arange(b - a)
                ramp = np.minimum(1.0, np.minimum(idx, idx[::-1]) / fade)
                fill = fill * (ramp[:, None] if layer.get("stereo") else ramp)
            if layer.get("stereo"):
                out[a:b] += amp * fill
                continue
            sig = np.zeros(n)
            sig[a:b] = amp * fill
        else:
            raise ValueError(f"unknown stimulus layer kind {kind!r}")
        channel = layer.get("channel", "both")
        for c in ((0, 1) if channel == "both" else (channel,)):
            out[:, c] += sig
    return out


def bela_events(scenario, sr, trigger_level=0.6):
    """The scenario's CV and gate events as (frame, channel, level, ramp)
    tuples, the form a bench driver replays: a trigger is a gate level at its
    start and zero `width` later, a hold likewise. Sorted by frame."""
    out = []
    for ev in scenario.events:
        ch = ev["channel"]
        if ev["kind"] == "cv":
            out.append((frames_of(ev["t"], sr), ch, ev["level"],
                        frames_of(ev.get("ramp", 0.0), sr)))
        elif ev["kind"] == "trigger":
            width = ev.get("width", 0.005)
            for t in series(ev["times"]):
                out += [(frames_of(t, sr), ch, trigger_level, 0),
                        (frames_of(t + width, sr), ch, 0.0, 0)]
        else:
            out += [(frames_of(ev["t0"], sr), ch, trigger_level, 0),
                    (frames_of(ev["t1"], sr), ch, 0.0, 0)]
    return sorted(out, key=lambda e: (e[0], e[1], e[2], e[3]))


def build(scenario, sr, seed):
    """Stimulus for `scenario` at `sr`; `seed` only feeds optional noise."""
    n = frames_of(scenario.seconds, sr) // BLOCK * BLOCK
    gen = scenario.generator
    kind = gen["generator"]
    if kind == "layers":
        audio = render_layers(gen["layers"], n, sr, seed).astype(np.float32)
    else:
        if kind == "sine":
            mono = _sine(n, sr, gen["freq"], gen["amp"])
        elif kind == "chirps":
            mono = _chirps(n, sr, gen["f0"], gen["f1"], gen["amp"],
                           gen["period_s"])
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


def history_seconds(scenario):
    """How long before the scenario a continuous tone was already playing: the
    longest `phase_s` of a sine layer that starts at zero. A tier that has no
    earlier run (the emulator) plays this lead-in first."""
    return max((layer.get("phase_s", 0.0)
                for layer in scenario.generator.get("layers", ())
                if layer["kind"] == "sine" and layer.get("start", 0.0) == 0.0
                and "every" not in layer), default=0.0)


def history(scenario, sr, seconds):
    """(n, 2) audio for the `seconds` before the scenario starts: each
    phase-shifted tone from the moment it began, in phase with the scenario's
    own render of it. Frames line up so the last history frame is the one
    before frame 0."""
    n = frames_of(seconds, sr)
    out = np.zeros((n, 2))
    for layer in scenario.generator.get("layers", ()):
        lead = layer.get("phase_s", 0.0)
        if (layer["kind"] != "sine" or not lead
                or layer.get("start", 0.0) != 0.0 or "every" in layer):
            continue
        start = max(0, n - frames_of(lead, sr))
        k = np.arange(n - start)
        w = np.ones(len(k))
        fade = min(frames_of(layer.get("fade_s", 0.01), sr), len(k))
        if fade:
            w[:fade] = np.linspace(0, 1, fade)
        sig = np.zeros(n)
        sig[start:] = layer["amp"] * w * np.sin(2 * np.pi * layer["freq"] * k / sr)
        channel = layer.get("channel", "both")
        for c in ((0, 1) if channel == "both" else (channel,)):
            out[:, c] += sig
    return out.astype(np.float32)

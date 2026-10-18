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


"""Shared rules: a render goes in, observables and a verdict per rule come out.

Alignment (timecodes, loopback lag) belongs to a tier adapter, not here: the
render handed to evaluate() starts at the stimulus start.
"""

import numpy as np

PASS, FAIL, MEASURE, INVALID = "PASS", "FAIL", "MEASURE", "INVALID"
SEVERITY = (PASS, MEASURE, FAIL, INVALID)  # worst last


def mono(x):
    return x.mean(axis=1) if x.ndim == 2 else x


def seg(x, sr, win):
    return x[int(win[0] * sr): int(win[1] * sr)]


def env(x, sr, win_s=0.005):
    w = max(1, int(win_s * sr))
    m = len(x) // w
    return np.sqrt(np.mean(x[: m * w].reshape(m, w) ** 2, axis=1)), w


def rms_db(x):
    return float(20 * np.log10(np.sqrt(np.mean(np.square(x))) + 1e-12))


def onsets(x, sr, floor_db=-40.0, min_gap_s=0.03):
    """Grain onsets: the 5 ms RMS envelope rising through a threshold 12 dB
    above its 10th percentile (and at least floor_db under the peak envelope),
    with a minimum gap between onsets."""
    e, w = env(mono(x), sr)
    if not len(e):
        return np.array([])
    edb = 20 * np.log10(e + 1e-9)
    thr = max(np.percentile(edb, 10) + 12.0, edb.max() + floor_db)
    above = edb > thr
    out, last = [], -1e9
    for i in np.nonzero(above[1:] & ~above[:-1])[0] + 1:
        t = i * w / sr
        if t - last >= min_gap_s:
            out.append(t)
            last = t
    return np.array(out)


def dominant(x, sr, fmin=60.0, fmax=4000.0):
    if len(x) < 64:
        return float("nan")
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x)), 4 * len(x)))
    f = np.fft.rfftfreq(4 * len(x), 1 / sr)
    sel = (f >= fmin) & (f <= fmax)
    return float(f[sel][np.argmax(spectrum[sel])])


def grains(x, sr, win):
    """[(onset, duration)] of each envelope run within 30 dB of the window's
    peak, in absolute seconds."""
    s = mono(seg(x, sr, win))
    e, w = env(s, sr)
    out = []
    if not len(e) or e.max() <= 0:
        return out
    above = 20 * np.log10(e + 1e-9) > 20 * np.log10(e.max()) - 30.0
    i = 0
    while i < len(above):
        if above[i]:
            j = i
            while j < len(above) and above[j]:
                j += 1
            if j - i >= 2:
                out.append((win[0] + i * w / sr, (j - i) * w / sr))
            i = j
        else:
            i += 1
    return out


def _verdict(ok):
    return PASS if ok else FAIL


def _silent(x, sr, p):
    peak_db = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
    # Energy over all channels: opposite-sign channels must not cancel.
    v = rms_db(seg(x, sr, p["win"])) - peak_db
    return dict(value_db_re_peak=v, verdict=_verdict(v < -45))


def _onsets_match_triggers(x, sr, p):
    on = onsets(seg(x, sr, p["win"]), sr) + p["win"][0]
    pending, lat = list(on), []
    for t in p["triggers"]:  # each onset answers at most one trigger
        match = next((o for o in pending if 0 <= o - t < 0.2), None)
        lat.append(None if match is None else match - t)
        if match is not None:
            pending.remove(match)
    hit = [v for v in lat if v is not None]
    extra = len(on) - len(hit)
    return dict(triggers=len(p["triggers"]), onsets=len(on), matched=len(hit),
                extra=extra, latency_ms=[round(1000 * v, 1) for v in hit],
                verdict=_verdict(len(hit) == len(p["triggers"]) and extra == 0))


def _onset_count(x, sr, p):
    n = len(onsets(seg(x, sr, p["win"]), sr))
    return dict(onsets=n, verdict=_verdict(n == p["expect"]))


def _onset_count_min(x, sr, p):
    n = len(onsets(seg(x, sr, p["win"]), sr))
    return dict(onsets=n, verdict=_verdict(n >= p["minimum"]))


def _rate_increasing(x, sr, p):
    rates = [len(onsets(seg(x, sr, w), sr)) / (w[1] - w[0]) for w in p["wins"]]
    ok = all(b >= a for a, b in zip(rates, rates[1:])) and rates[-1] > rates[0]
    return dict(rates_hz=rates, verdict=_verdict(ok))


def _duration_monotonic(x, sr, p):
    d = [float(np.median([g[1] for g in grains(x, sr, w)] or [np.nan]))
         for w in p["wins"]]
    return dict(median_duration_s=d,
                verdict=_verdict(all(b > a for a, b in zip(d, d[1:]))))


def _direction(x, sr, p):
    """One slope per trigger: dominant frequency in 60 ms frames (20 ms hop)
    from the trigger to the next one, keeping frames within 15 dB of the
    loudest (the newest grain), then a Theil-Sen slope in Hz/s."""
    ts = sorted(p["triggers"]) + [p["win"][1]]
    per = []
    for t0, t1 in zip(ts, ts[1:]):
        a, b = t0 + 0.01, min(t1, t0 + 0.9) - 0.02
        frames = [(c, seg(x, sr, (c, c + 0.06)))
                  for c in np.arange(a, b - 0.06, 0.02)]
        lv = np.array([rms_db(mono(s)) for _, s in frames])
        keep = [(c, dominant(mono(s), sr))
                for (c, s), level in zip(frames, lv) if level >= lv.max() - 15.0]
        if len(keep) < 4:
            per.append(float("nan"))
            continue
        tt, ff = np.array(keep).T
        i, j = np.triu_indices(len(tt), 1)
        per.append(float(np.median((ff[j] - ff[i]) / (tt[j] - tt[i]))))
    fin = [v for v in per if np.isfinite(v)]
    m = float(np.median(fin)) if fin else float("nan")
    got = "forward" if m > 0 else "reverse" if m < 0 else "unknown"
    agree = all(np.sign(v) == np.sign(m) for v in fin)
    ok = got == p["expect"] and agree and len(fin) == len(per)
    return dict(per_trigger_slope_hz_per_s=per, median_slope_hz_per_s=m,
                direction=got, verdict=_verdict(ok))


RULES = {
    "silent": _silent,
    "onsets_match_triggers": _onsets_match_triggers,
    "onset_count": _onset_count,
    "onset_count_min": _onset_count_min,
    "rate_increasing": _rate_increasing,
    "duration_monotonic": _duration_monotonic,
    "direction": _direction,
}


def evaluate(rules, x, sr):
    """One result dict per rule entry; an unknown rule is INVALID, not an
    exception, so one bad rule cannot hide the others."""
    results = []
    for entry in rules:
        params = {k: v for k, v in entry.items()
                  if k not in ("rule", "behaviours")}
        r = dict(rule=entry["rule"], behaviours=list(entry.get("behaviours", ())),
                 params={k: v for k, v in params.items() if k != "triggers"})
        fn = RULES.get(entry["rule"])
        if fn is None:
            r.update(verdict=INVALID, error=f"unknown rule {entry['rule']!r}")
        else:
            r.update(fn(x, sr, params))
        results.append(r)
    return results


def overall(verdicts):
    return max(verdicts, key=SEVERITY.index) if verdicts else INVALID

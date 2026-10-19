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
render handed to evaluate() starts at the stimulus start, or a tier passes the
residual path latency as `offset_s`, which shifts every stimulus time (windows,
trigger times) and nothing else.

Each rule is `fn(x, sr, p, offset_s=0.0, ctx=None)` and returns a dict of
observables plus a verdict. `p` is the rule's parameters; `ctx` carries what
only a tier knows (noise floor, clipping record, stimulus clock). Rules never
read files and never depend on which tier produced the render.
"""

import copy

import numpy as np

from repro import beta_rules, stimulus

PASS, FAIL, MEASURE, INVALID = "PASS", "FAIL", "MEASURE", "INVALID"
INCONCLUSIVE, ERROR = "INCONCLUSIVE", "ERROR"
SEVERITY = (PASS, MEASURE, INCONCLUSIVE, FAIL, ERROR, INVALID)  # worst last
WINDOW_KEYS = ("win", "quiet", "loud", "base", "test")


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


def band_db(x, sr, freqs, bw=0.03):
    """Power in dB around each frequency and its second harmonic."""
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / sr)
    total = 0.0
    for fc in freqs:
        for h in (1, 2):
            sel = np.abs(f - h * fc) <= bw * h * fc
            total += spectrum[sel].sum()
    return float(10 * np.log10(total + 1e-20))


def dominant(x, sr, fmin=60.0, fmax=4000.0):
    if len(x) < 64:
        return float("nan")
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x)), 4 * len(x)))
    f = np.fft.rfftfreq(4 * len(x), 1 / sr)
    sel = (f >= fmin) & (f <= fmax)
    return float(f[sel][np.argmax(spectrum[sel])])


def grains(x, sr, win):
    """[(onset, duration, attack_fraction, slope)] of each envelope run within
    30 dB of the window's peak, onset in absolute seconds; slope is the
    dominant-frequency change from the first to the second half."""
    s = mono(seg(x, sr, win))
    e, w = env(s, sr)
    out = []
    if not len(e) or e.max() <= 0:
        return out
    edb = 20 * np.log10(e + 1e-9)
    above = edb > edb.max() - 30.0
    i = 0
    while i < len(above):
        if above[i]:
            j = i
            while j < len(above) and above[j]:
                j += 1
            if j - i >= 2:
                pk = i + int(np.argmax(e[i:j]))
                g = s[i * w: j * w]
                h = len(g) // 2
                slope = (dominant(g[h:], sr) - dominant(g[:h], sr)
                         if h > 256 else float("nan"))
                out.append((win[0] + i * w / sr, (j - i) * w / sr,
                            (pk - i) / max(1, j - i), slope))
            i = j
        else:
            i += 1
    return out


def _verdict(ok):
    return PASS if ok else FAIL


def _shift(win, offset_s):
    return (win[0] + offset_s, win[1] + offset_s)


def _windows(p):
    wins = [tuple(p[k]) for k in WINDOW_KEYS if k in p]
    return wins + [tuple(w) for w in p.get("wins", [])]


# ---------------------------------------------------------------------------
# level rules
# ---------------------------------------------------------------------------
def _silent(x, sr, p, offset_s=0.0, ctx=None):
    peak_db = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
    # Energy over all channels: opposite-sign channels must not cancel.
    v = rms_db(seg(x, sr, _shift(p["win"], offset_s))) - peak_db
    return dict(value_db_re_peak=v, verdict=_verdict(v < -45))


def _not_silent(x, sr, p, offset_s=0.0, ctx=None):
    v = rms_db(seg(x, sr, _shift(p["win"], offset_s)))
    return dict(rms_dbfs=v, verdict=_verdict(v > -50))


def _level_step(x, sr, p, offset_s=0.0, ctx=None):
    quiet = rms_db(mono(seg(x, sr, _shift(p["quiet"], offset_s))))
    loud = rms_db(mono(seg(x, sr, _shift(p["loud"], offset_s))))
    return dict(step_db=loud - quiet, input_step_db=p["step_db"], verdict=MEASURE,
                rule_text="a smaller output step than input step suggests "
                          "compression; it does not show which stage acted")


def _dry_passes(x, sr, p, offset_s=0.0, ctx=None):
    s = mono(seg(x, sr, _shift(p["win"], offset_s)))
    f = dominant(s, sr)
    if rms_db(s) < -120:  # digital silence has no dominant frequency to speak of
        return dict(dominant_hz=f, verdict=FAIL, reason="window is silent")
    return dict(dominant_hz=f, verdict=_verdict(abs(f / p["freq"] - 1) < 0.01))


def _band_ratio(x, sr, p, offset_s=0.0, ctx=None):
    s = mono(seg(x, sr, _shift(p["win"], offset_s)))
    d = band_db(s, sr, p["present"]) - band_db(s, sr, p["absent"])
    return dict(present_minus_absent_db=d, verdict=_verdict(d >= p["min_db"]))


def _tail_increase(x, sr, p, offset_s=0.0, ctx=None):
    def tail(w):
        e, _ = env(mono(seg(x, sr, _shift(w, offset_s))), sr, 0.05)
        return float(20 * np.log10(np.percentile(e, 30) + 1e-9))
    return dict(tail_change_db=tail(p["test"]) - tail(p["base"]), verdict=MEASURE,
                caveat="a tail change is reported, not judged against a threshold")


# ---------------------------------------------------------------------------
# onset and rate rules
# ---------------------------------------------------------------------------
def _onsets_match_triggers(x, sr, p, offset_s=0.0, ctx=None):
    win = _shift(p["win"], offset_s)
    on = onsets(seg(x, sr, win), sr) + win[0]
    guard = p.get("timing_guard_s", 0.0)
    trig = np.array(sorted(p["triggers"])) + offset_s
    pending, lat = list(on), []
    for t in trig:  # each onset answers at most one trigger
        match = next((o for o in pending if -guard <= o - t < 0.2 + guard), None)
        lat.append(None if match is None else match - t)
        if match is not None:
            pending.remove(match)
    hit = [v for v in lat if v is not None]
    extra = len(on) - len(hit)
    return dict(triggers=len(trig), onsets=len(on), matched=len(hit), extra=extra,
                latency_ms=[round(1000 * v, 1) for v in hit],
                verdict=_verdict(len(hit) == len(trig) and extra == 0))


def _onset_count(x, sr, p, offset_s=0.0, ctx=None):
    n = len(onsets(seg(x, sr, _shift(p["win"], offset_s)), sr))
    return dict(onsets=n, verdict=_verdict(n == p["expect"]))


def _onset_count_min(x, sr, p, offset_s=0.0, ctx=None):
    n = len(onsets(seg(x, sr, _shift(p["win"], offset_s)), sr))
    return dict(onsets=n, verdict=_verdict(n >= p["minimum"]))


def _rates(x, sr, p, offset_s):
    return [len(onsets(seg(x, sr, _shift(w, offset_s)), sr)) / (w[1] - w[0])
            for w in p["wins"]]


def _rate_increasing(x, sr, p, offset_s=0.0, ctx=None):
    """Four windows: the rate rises strictly over the first three, and the
    fourth stays within `plateau` (default 0.85) of the best of them.
    Overlapping grains lift the onset floor, so a rising density can level off
    rather than keep climbing."""
    rates = _rates(x, sr, p, offset_s)
    ok = (len(rates) == 4 and rates[0] < rates[1] < rates[2]
          and rates[3] >= p.get("plateau", 0.85) * max(rates[:3]))
    return dict(rates_hz=rates, verdict=_verdict(ok))


def _rate_order(x, sr, p, offset_s=0.0, ctx=None):
    rates = _rates(x, sr, p, offset_s)
    return dict(rates_hz=rates,
                verdict=_verdict(0 < rates[0] < rates[1] <= p["clock_hz"] * 1.1))


def _rate_divided(x, sr, p, offset_s=0.0, ctx=None):
    rates = _rates(x, sr, p, offset_s)
    ok = (all(0 < v < p["clock_hz"] * 0.9 for v in rates)
          and abs(rates[1] - rates[0]) > 0.2)
    over = [p["clock_hz"] / v if v else None for v in rates]
    want = p.get("ratios")
    if want is not None:
        tol = p.get("ratio_tol", 0.2)
        ok = (ok and len(want) == len(over) and all(
            o is not None and abs(o - w) <= tol * w for o, w in zip(over, want)))
    return dict(rates_hz=rates, clock_over_rate=over, verdict=_verdict(ok),
                rule_text="both settings divide the clock, by different ratios"
                          + (f" (expected {want})" if want is not None else ""))


# ---------------------------------------------------------------------------
# grain shape and pitch rules
# ---------------------------------------------------------------------------
def _need_windows(p, count=2):
    if len(p["wins"]) < count:
        raise ValueError(f"needs at least {count} windows")


def _duration_monotonic(x, sr, p, offset_s=0.0, ctx=None):
    _need_windows(p)
    d = [float(np.median([g[1] for g in
                          grains(x, sr, _shift(w, offset_s))] or [np.nan]))
         for w in p["wins"]]
    return dict(median_duration_s=d,
                verdict=_verdict(all(b > a for a, b in zip(d, d[1:]))))


def _direction(x, sr, p, offset_s=0.0, ctx=None):
    """Playback direction from a rising chirp. With `triggers`, one slope per
    trigger: dominant frequency in 60 ms frames (20 ms hop) from the trigger to
    the next one, keeping frames within 15 dB of the loudest (the newest
    grain), then a Theil-Sen slope in Hz/s. Without, the median half-to-half
    frequency change of the window's grains."""
    if "triggers" in p:
        ts = sorted(p["triggers"]) + [p["win"][1]]
        per = []
        for t0, t1 in zip(ts, ts[1:]):
            a = t0 + offset_s + 0.01
            b = min(t1, t0 + 0.9) + offset_s - 0.02
            frames = [(c, seg(x, sr, (c, c + 0.06)))
                      for c in np.arange(a, b - 0.06, 0.02)]
            lv = (np.array([rms_db(mono(s)) for _, s in frames])
                  if frames else np.array([]))
            keep = [(c, dominant(mono(s), sr))
                    for (c, s), level in zip(frames, lv)
                    if level >= lv.max() - 15.0]
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
    sl = [g[3] for g in grains(x, sr, _shift(p["win"], offset_s))
          if np.isfinite(g[3]) and g[1] >= 0.1]
    m = float(np.median(sl)) if sl else float("nan")
    got = "forward" if m > 0 else "reverse" if m < 0 else "unknown"
    return dict(median_freq_slope_hz=m, direction=got,
                verdict=_verdict(got == p["expect"]))


def _pitch_ratio(x, sr, p, offset_s=0.0, ctx=None):
    def median_hz(win):
        return np.median([dominant(mono(seg(x, sr, (on, on + 0.15))), sr)
                          for on, *_ in grains(x, sr, _shift(win, offset_s))]
                         or [np.nan])
    fb, ft = median_hz(p["base"]), median_hz(p["test"])
    ratio = ft / fb
    return dict(base_hz=fb, test_hz=ft, ratio=ratio,
                verdict=_verdict(abs(ratio / p["ratio"] - 1) <= p["tol"]))


def _pitch_shift(x, sr, p, offset_s=0.0, ctx=None):
    fb = dominant(mono(seg(x, sr, _shift(p["base"], offset_s))), sr)
    ft = dominant(mono(seg(x, sr, _shift(p["test"], offset_s))), sr)
    return dict(base_hz=fb, test_hz=ft, ratio=ft / fb, verdict=MEASURE)


def _attack_slow_vs_bell(x, sr, p, offset_s=0.0, ctx=None):
    af = [float(np.median([g[2] for g in
                           grains(x, sr, _shift(w, offset_s))] or [np.nan]))
          for w in p["wins"]]
    ok = np.isfinite(af[1]) and np.isfinite(af[2]) and af[2] > af[1]
    return dict(attack_fraction=af, verdict=_verdict(ok),
                caveat="rectangular time-to-peak is diagnostic only")


def _age_increasing(x, sr, p, offset_s=0.0, ctx=None):
    """Input steps through known tones, so a grain's tone gives its age modulo
    the cycle. `capacity_s` (the buffer length, optional) excuses a plateau at
    the buffer end, and a capacity of 8 s or more cannot be resolved by an 8 s
    tone cycle, so it is reported as MEASURE."""
    ctx = ctx or {}
    _need_windows(p)
    tones = np.array(stimulus.tones(p["tones"]))
    stim_offset = ctx.get("stimulus_offset_s", 0.0)
    ratio = ctx.get("stimulus_rate_ratio", 1.0)
    ages = []
    for w in p["wins"]:
        found = []
        for on, du, _, _ in grains(x, sr, _shift(w, offset_s)):
            f = dominant(mono(seg(x, sr, (on, on + min(du, 0.2)))), sr)
            k = int(np.argmin(np.abs(tones - f)))
            played = int(round((on - offset_s - stim_offset) /
                               (p["step_s"] * ratio)))
            found.append(((played - k) % len(tones)) * p["step_s"])
        ages.append(float(np.median(found)) if found else float("nan"))
    capacity = ctx.get("capacity_s", p.get("capacity_s"))
    if capacity is not None:
        ok = all(b > a or (abs(a - capacity) <= p["step_s"]
                           and abs(b - capacity) <= p["step_s"])
                 for a, b in zip(ages, ages[1:]))
    else:
        ok = all(b > a for a, b in zip(ages, ages[1:]))
    verdict = MEASURE if capacity is not None and capacity >= 8 else _verdict(ok)
    return dict(median_age_s_mod_8s=ages, stereo_capacity_s=capacity,
                verdict=verdict,
                caveat="an 8 s tone cycle aliases buffers of 8 s or more")


# ---------------------------------------------------------------------------
# delay rules
# ---------------------------------------------------------------------------
def _loop_period(x, sr, p, offset_s=0.0, ctx=None):
    """Autocorrelation of the 2 ms envelope: the period of a repeating loop.
    ac[k]/ac[0] near 1 is an exact repeat; a silent window scores near 0 and
    its argmax is arbitrary, so a low peak is INCONCLUSIVE."""
    s = mono(seg(x, sr, _shift(p["win"], offset_s)))
    e, w = env(s, sr, 0.002)
    e = e - e.mean()
    ac = np.correlate(e, e, "full")[len(e) - 1:]
    lo, hi = int(0.1 * sr / w), int(1.5 * sr / w)
    k = lo + int(np.argmax(ac[lo:hi]))
    per = k * w / sr
    peak = float(ac[k] / ac[0]) if ac[0] > 0 else 0.0
    verdict = (INCONCLUSIVE if peak < p.get("min_ac_peak", 0.3)
               else _verdict(abs(per - p["period"]) <= p["tol"]))
    return dict(period_s=per, ac_peak=peak, verdict=verdict)


def _echo_peak_ratio(x, sr, p, offset_s=0.0, ctx=None):
    guard = p.get("timing_guard_s", 0.0)

    def delay(tb):
        lo = .08
        s = mono(seg(x, sr, (tb + offset_s + lo, tb + offset_s + 2.9 + guard)))
        e, w = env(s, sr, 0.002)
        return lo + int(np.argmax(e)) * w / sr if len(e) else float("nan")
    da = [delay(t) for t in p["bursts_a"]]
    db = [delay(t) for t in p["bursts_b"]]
    return dict(echo_delay_s_a=da, echo_delay_s_b=db,
                ratio=float(np.median(da) / np.median(db)), verdict=MEASURE,
                rule_text="peak locations are not the base delay: echoes may "
                          "sit at multiples of TIME")


RULES = {
    "silent": _silent,
    "not_silent": _not_silent,
    "level_step": _level_step,
    "dry_passes": _dry_passes,
    "band_ratio": _band_ratio,
    "tail_increase": _tail_increase,
    "onsets_match_triggers": _onsets_match_triggers,
    "onset_count": _onset_count,
    "onset_count_min": _onset_count_min,
    "rate_increasing": _rate_increasing,
    "rate_order": _rate_order,
    "rate_divided": _rate_divided,
    "duration_monotonic": _duration_monotonic,
    "direction": _direction,
    "pitch_ratio": _pitch_ratio,
    "pitch_shift": _pitch_shift,
    "attack_slow_vs_bell": _attack_slow_vs_bell,
    "age_increasing": _age_increasing,
    "loop_period": _loop_period,
    "echo_period": _loop_period,
    "echo_peak_ratio": _echo_peak_ratio,
}

# ---------------------------------------------------------------------------
# beta feature rules (see beta_rules.py): they read absolute stimulus times, so
# the render must already be aligned to the stimulus start.
# ---------------------------------------------------------------------------
DESTINATIONS = ("feedback", "dry_wet", "reverb")


def _aligned(offset_s):
    if offset_s:
        raise ValueError("this rule needs a render aligned to the stimulus "
                         "start (offset_s must be 0)")


def _random_side_statistics(x, sr, p, offset_s=0.0, ctx=None):
    _aligned(offset_s)
    return beta_rules.measure(x, sr, dict(random_control=p["control"]))


def _assign_cv_effect(x, sr, p, offset_s=0.0, ctx=None):
    _aligned(offset_s)
    return beta_rules.measure(x, sr, dict(
        assignable_cv_destination=DESTINATIONS.index(p["destination"])))


def _grain_pitch_scale(x, sr, p, offset_s=0.0, ctx=None):
    _aligned(offset_s)
    names = [name for name, _ in beta_rules.SCALES]
    measurement = beta_rules.measure(x, sr, dict(random_control="pitch"))
    return beta_rules.scale_measurement(measurement,
                                        0xa1 + names.index(p["scale"]))


def _dry_crosstalk(x, sr, p, offset_s=0.0, ctx=None):
    _aligned(offset_s)
    noise = (ctx or {}).get("noise_dbfs")
    if noise is None:
        return dict(verdict=INCONCLUSIVE, reason="needs the run's noise floor")
    return beta_rules.dry_crosstalk(x, sr, noise_dbfs=noise)


def _clouds_ab(x, sr, p, offset_s=0.0, ctx=None):
    _aligned(offset_s)
    ctx = ctx or {}
    if "transitions" not in ctx or "start_epoch" not in ctx:
        return dict(verdict=INCONCLUSIVE,
                    reason="needs the tier's switch times (transitions, "
                           "start_epoch)")
    return beta_rules.clouds_ab(x, sr, ctx["transitions"], ctx["start_epoch"])


RULES.update({
    "random_side_statistics": _random_side_statistics,
    "assign_cv_effect": _assign_cv_effect,
    "grain_pitch_scale": _grain_pitch_scale,
    "dry_crosstalk": _dry_crosstalk,
    "clouds_ab": _clouds_ab,
})

# Rules that read the output level directly need a known noise floor.
NOISE_GATED = ("loop_period", "echo_period", "pitch_shift")
MIN_NOISE_MARGIN_DB = 20.0


def noise_margin(x, sr, p, offset_s, noise_dbfs):
    """The window levels against the run's noise floor (stereo, no credit for
    mono cancellation). The result carries a verdict only when a window sits
    less than 20 dB over the floor, or the floor is missing: INCONCLUSIVE."""
    windows = [p["win"]] if "win" in p else [p["base"], p["test"]]
    floor = (np.asarray(noise_dbfs, dtype=float) if noise_dbfs is not None
             else np.array([]))
    levels = [rms_db(mono(seg(x, sr, _shift(w, offset_s)))) for w in windows]
    valid = floor.size > 0 and np.isfinite(floor).all()
    margin = [v - float(floor.max()) for v in levels] if valid else []
    out = dict(window_rms_dbfs=levels, noise_dbfs=floor.tolist(),
               level_margin_db=margin, minimum_level_margin_db=MIN_NOISE_MARGIN_DB)
    if not valid or any(not np.isfinite(v) or v < MIN_NOISE_MARGIN_DB
                        for v in margin):
        out.update(verdict=INCONCLUSIVE,
                   reason="analysis window below run noise floor +20 dB "
                          "or noise missing")
    return out


def clip_check(x, sr, p, offset_s, raw, raw_sr):
    """None, or an INVALID result when more than 0.1% of the samples in the
    rule's windows reached full scale in the raw capture."""
    clipped = sum(int(np.sum(np.abs(seg(raw, raw_sr, _shift(w, offset_s)))
                             >= 0.999)) for w in _windows(p))
    total = sum(seg(raw, raw_sr, _shift(w, offset_s)).size
                for w in _windows(p)) or 1
    if clipped > 0.001 * total:
        return dict(verdict=INVALID,
                    error=f"recording clipped at the input: {clipped} samples "
                          "in the rule's windows")
    return None


def evaluate_one(entry, x, sr, offset_s=0.0, ctx=None):
    """One rule entry (a dict with `rule`, optional `behaviours`, and its
    parameters) to a result dict. An unknown rule is INVALID, not an
    exception, and a rule that cannot be computed is ERROR, never a pass.

    ctx keys (all optional): `noise_dbfs` (present means the noise-floor check
    applies, even when None), `clip_guard` with `clip_audio`/`clip_sr`,
    `stimulus_offset_s`, `stimulus_rate_ratio`, `capacity_s`."""
    ctx = ctx or {}
    params = {k: v for k, v in entry.items() if k not in ("rule", "behaviours")}
    r = dict(rule=entry["rule"], behaviours=list(entry.get("behaviours", ())),
             params={k: v for k, v in params.items() if k != "triggers"})
    fn = RULES.get(entry["rule"])
    if fn is None:
        r.update(verdict=INVALID, error=f"unknown rule {entry['rule']!r}")
        return r
    if entry["rule"] in NOISE_GATED and "noise_dbfs" in ctx:
        gated = noise_margin(x, sr, params, offset_s, ctx["noise_dbfs"])
        r.update(gated)
        if "verdict" in gated:
            return r
    if ctx.get("clip_guard"):
        raw = x if ctx.get("clip_audio") is None else ctx["clip_audio"]
        raw_sr = sr if ctx.get("clip_sr") is None else ctx["clip_sr"]
        clipped = clip_check(x, sr, params, offset_s, raw, raw_sr)
        if clipped is not None:
            r.update(clipped)
            return r
    try:
        r.update(fn(x, sr, params, offset_s, ctx))
    except Exception as exc:  # reported, never silently passed
        r.update(verdict=ERROR, error=repr(exc))
    return r


def evaluate(rules, x, sr, offset_s=0.0, ctx=None):
    """One result dict per rule entry, so one bad rule cannot hide the others."""
    return [evaluate_one(entry, x, sr, offset_s, ctx) for entry in rules]


def overall(verdicts):
    return max(verdicts, key=SEVERITY.index) if verdicts else INVALID


def session_verdict(verdicts):
    """The bench reduction's roll-up: FAIL, else INCONCLUSIVE, else MEASURE,
    else PASS (an ERROR or INVALID rule is turned INCONCLUSIVE before this)."""
    verdicts = set(verdicts)
    return next((v for v in (FAIL, INCONCLUSIVE, MEASURE) if v in verdicts), PASS)


# ---------------------------------------------------------------------------
# noise floor, lead-in and relative-silence rules
# ---------------------------------------------------------------------------
def noise_threshold(x, sr, onset):
    """Per-channel RMS (dBFS) of the quiet lead-in before `onset`, the
    interface noise floor. A lead-in that is not quiet is residual activity,
    not a noise estimate, so it is refused. A scenario in which the module
    plays itself (self-play) has no quiet lead-in and never calls this; its
    rules use no noise floor."""
    end = round((onset - .05) * sr)
    if end < round(.1 * sr):
        raise ValueError('missing quiet lead-in (minimum 100 ms)')
    start = max(0, round((onset - .35) * sr))
    if end - start < round(.1 * sr):
        raise ValueError('missing quiet lead-in')
    noise = 20 * np.log10(np.maximum(
        np.sqrt(np.mean(x[start:end] ** 2, axis=0)), 1e-15))
    if not np.isfinite(noise).all() or np.any(noise > -80) or np.any(noise + 10 >= -50):
        raise ValueError('lead-in not quiet enough for provisional noise threshold')
    return noise


def noise_for_scenario(x, sr, onset, scenario):
    """Noise floor for a scenario, or None for self-play."""
    return None if scenario == 'self-play' else noise_threshold(x, sr, onset)


def silence_reference(x, sr, rules, offset=0.):
    """Per-channel level (dB) of the loudest measured window among the
    non-silent rules; silence is judged relative to it. `rules` are
    (kind, ids, params) tuples."""
    windows = []
    for kind, _, params in rules:
        if kind == 'silent':
            continue
        windows += [v for k, v in params.items() if k in WINDOW_KEYS]
        windows += list(params.get('wins', []))
    levels = []
    for a, b in windows:
        z = seg(x, sr, (a + offset, b + offset))
        if len(z):
            levels.append(20 * np.log10(np.maximum(
                np.sqrt(np.mean(z * z, axis=0)), 1e-15)))
    if not levels or np.any(np.max(levels, axis=0) < -50):
        raise ValueError('missing active rule window for relative silence')
    return np.max(levels, axis=0)


def provisional_level_rule(segment, kind, aligned, sr, rules, offset=0., media=3):
    """silent: 45 dB (35 dB for the noisiest medium) under the loudest rule
    window on every channel. not_silent: above -50 dBFS on some channel."""
    db = 20 * np.log10(np.maximum(np.sqrt(np.mean(segment ** 2, axis=0)), 1e-15))
    threshold = (silence_reference(aligned, sr, rules, offset)
                 - (35. if media == 0 else 45.)
                 if kind == 'silent' else np.full(2, -50.))
    passed = np.all(db < threshold) if kind == 'silent' else np.any(db > threshold)
    return dict(rule=kind, rms_dbfs=db.tolist(), threshold_dbfs=threshold.tolist(),
                provisional=True, verdict=PASS if passed else FAIL)


def guarded_rule(rule, guard):
    """A (kind, ids, params) rule with its windows moved off the uncertain
    transition neighbourhood: stable-state windows shrink by `guard`,
    onset-match windows grow by it, onset-count windows stay (their boundary
    onsets are reported as INCONCLUSIVE by onset_count_rule)."""
    rule = copy.deepcopy(rule)
    kind, ids, p = rule
    expand = kind == 'onsets_match_triggers'
    if kind in ('onsets_match_triggers', 'echo_peak_ratio'):
        p['timing_guard_s'] = guard

    def window(w):
        if kind in ('onset_count', 'onset_count_min'):
            return tuple(w)
        a, b = w
        result = (a - guard, b + guard) if expand else (a + guard, b - guard)
        if result[0] >= result[1]:
            raise ValueError('timing guard consumes measurement window')
        return result
    for key in WINDOW_KEYS:
        if key in p:
            p[key] = window(p[key])
    if 'wins' in p:
        p['wins'] = [window(w) for w in p['wins']]
    return rule


def onset_count_rule(x, sr, p, offset, guard, kind):
    a, b = p['win']
    a += offset
    b += offset
    on = onsets(seg(x, sr, (a - guard, b + guard)), sr) + a - guard
    boundary = on[(np.abs(on - a) <= guard) | (np.abs(on - b) <= guard)]
    count = int(np.sum((on >= a) & (on < b)))
    passed = count == p['expect'] if kind == 'onset_count' else count >= p['minimum']
    return dict(rule=kind, onsets=count, boundary_onsets_s=boundary.tolist(),
                verdict=INCONCLUSIVE if len(boundary) else PASS if passed else FAIL)

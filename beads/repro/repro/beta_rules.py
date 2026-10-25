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
"""Rules for the beta feature scenarios: crosstalk, reverb switching, scale
quantizing, and the statistics of random side effects. Pure functions of a
render; they take an (n, 2) float array and its sample rate."""

import numpy

from repro.scales import SCALES, TRIGGERS  # noqa: F401

# A render is clipped when more than 0.1 % of its samples reach full scale (the
# same limit as rules.clip_check). The output limiter lets a lone sample
# overshoot and the converter clamps it, so a handful of full-scale samples is
# not a clipped render.
CLIP_LEVEL = .999
CLIP_FRACTION = .001

# clouds_ab thresholds (decibels).
HF_CHANGE_MIN_DB = .5
MAX_CONTROL_VARIATION_DB = 1.
STEP_MAX_DB = 20.
DB_FLOOR = -120.
SILENCE = 1e-6  # -120 dBFS: below this a level is digital silence


def db(value, reference):
  """20 log10(value / reference), floored: a fade to digital silence is a
    large dip, not minus infinity."""
  return float(max(20 * numpy.log10(max(value, 1e-15) / reference), DB_FLOOR))


def clipped(x):
  return bool(numpy.mean(numpy.abs(x) >= CLIP_LEVEL) > CLIP_FRACTION)


def dry_crosstalk(audio, sr, *, noise_dbfs):
  x = numpy.asarray(audio, float)
  if x.ndim != 2 or x.shape[1] != 2 or not numpy.isfinite(x).all():
    raise ValueError('invalid stereo capture')
  rows = []
  for driven, a, b in ((0, 3., 7.), (1, 11., 15.)):
    y = x[round(a * sr):round(b * sr)]
    if len(y) != round((b - a) * sr) or clipped(y):
      return dict(rule='dry_crosstalk',
                  verdict='INCONCLUSIVE',
                  reason='coverage or clipping')
    rms = numpy.sqrt(numpy.mean(y * y, axis=0))
    if rms[driven] < .005:
      return dict(rule='dry_crosstalk',
                  verdict='INCONCLUSIVE',
                  reason='driven channel too quiet')
    # Noise-inclusive upper bound: a quiet undriven channel can establish one.
    upper = max(rms[1 - driven], 10**(float(noise_dbfs[1 - driven]) / 20))
    db = float(20 * numpy.log10(max(upper, 1e-15) / rms[driven]))
    rows.append(
        dict(driven=driven,
             crosstalk_upper_db=db,
             threshold_db=-50.,
             verdict='PASS' if db <= -50 else 'FAIL'))
  return dict(rule='dry_crosstalk',
              verdict='PASS' if all(r['verdict'] == 'PASS'
                                    for r in rows) else 'FAIL',
              channels=rows)


def clouds_ab(audio, sr, transitions, start_epoch):
  x = numpy.asarray(audio, float)

  def invalid(reason):
    return dict(rule='clouds_ab', verdict='INCONCLUSIVE', reason=reason)

  if x.ndim != 2 or x.shape[1] != 2 or not numpy.isfinite(x).all() or len(
      x) < 149 * sr or clipped(x):
    return invalid('invalid, clipped or incomplete audio')
  switches = transitions.get('switches', [])
  if len(switches) != 2 or [r.get('value') for r in switches] != [0xa2, 0]:
    return invalid('missing A/B/A switches')
  bands = []
  levels = []
  for a, b in ((12, 19), (20, 27), (65, 72), (130, 137)):
    y = x[round(a * sr):round(b * sr)]
    rms = float(numpy.sqrt(numpy.mean(y * y)))
    if rms < .0001:
      return invalid('inaudible grains or tail')
    levels.append(rms)
    power = numpy.abs(numpy.fft.rfft(y * numpy.hanning(len(y))[:, None],
                                     axis=0))**2
    f = numpy.fft.rfftfreq(len(y), 1 / sr)
    low = float(power[(f >= 200) & (f < 2000)].sum())
    high = float(power[(f >= 4000) & (f < 10000)].sum())
    if low < 1e-12 or high < 1e-12:
      return invalid('unresolved HF bands')
    bands.append(high / low)
  control_variation = float(abs(10 * numpy.log10(bands[1] / bands[0])))
  # Two windows of the same state that disagree this much say the grains are
  # too few to compare states, not that the firmware misbehaves.
  if control_variation > MAX_CONTROL_VARIATION_DB:
    return invalid('no-switch windows disagree')
  bands = bands[1:]

  # A no-switch A window supplies a baseline for stochastic grain-envelope dips.
  def dip_db(y, base):
    width = max(1, round(.005 * sr))
    n = len(y) // width
    if n < 1:
      raise ValueError('short fade window')
    rms = numpy.sqrt(
        numpy.mean(y[:n * width].reshape(n, width, 2)**2, axis=(1, 2)))
    return db(float(rms.min()), base)

  control_base = float(numpy.sqrt(numpy.mean(x[11 * sr:12 * sr]**2)))
  if control_base < .0001:
    return invalid('quiet no-switch control')
  control_dip = dip_db(x[12 * sr:19 * sr], control_base)
  # A silent gap in the control leaves no room to show a deeper fade.
  if control_dip < -40:
    return invalid('no-switch control has a silent gap')
  rows = []
  for r in switches:
    # Epoch brackets come from the actual writer invocation and verified
    # convergence.
    a = r['before_epoch'] - start_epoch
    b = r['after_epoch'] - start_epoch
    if r.get('rc') != 0 or not 0 <= a < b < len(x) / sr - 1 or b - a > 30:
      return invalid('invalid switch timing')
    y = x[round(a * sr):round((b + .25) * sr)]
    width = max(1, round(.005 * sr))
    n = len(y) // width
    rms = numpy.sqrt(
        numpy.mean(y[:n * width].reshape(n, width, 2)**2, axis=(1, 2)))
    base = float(numpy.sqrt(numpy.mean(x[round((a - 1) * sr):round(a *
                                                                   sr)]**2)))
    if base < .0001:
      return invalid('quiet switch reference')
    dip = db(float(rms.min()), base)
    # The fade is the run of bins around the quietest one that sit 6 dB or
    # more under the reference. Click checks look at the fade going down
    # (15 ms before the run to 10 ms into it), not at what follows: a grain
    # that starts once the style has changed begins with a hard onset of
    # its own. A pop inside the silent gap is therefore not seen here.
    lo = hi = int(numpy.argmin(rms))
    quiet = base * 10**(-6 / 20)
    while lo > 0 and rms[lo - 1] <= quiet:
      lo -= 1
    while hi < n - 1 and rms[hi + 1] <= quiet:
      hi += 1
    z = y[max(0, lo - 3) * width:min(n, lo + 2) * width]
    peak = float(numpy.max(abs(z)))
    crest = db(peak, base)
    step = float(numpy.max(abs(numpy.diff(z, axis=0)), initial=0))
    step_db = db(step, base)
    rows.append(
        dict(fade_dip_db=dip,
             crest_db=crest,
             max_sample_step=step,
             step_db=step_db,
             control_dip_db=control_dip,
             verdict='PASS' if dip <= min(-6, control_dip - 3.) and
             crest <= 20 and step_db <= STEP_MAX_DB else 'FAIL'))
  change = float(10 * numpy.log10(bands[1] / numpy.sqrt(bands[0] * bands[2])))
  recovery = float(abs(10 * numpy.log10(bands[2] / bands[0])))
  # The state change moves the high band by about a decibel on this noise, so
  # the bar is half a decibel and beyond the scatter between no-switch windows.
  hf_max = -max(HF_CHANGE_MIN_DB, control_variation)
  passed = change <= hf_max and recovery <= 3. and all(r['verdict'] == 'PASS'
                                                       for r in rows)
  return dict(
      rule='clouds_ab',
      verdict='PASS' if passed else 'FAIL',
      hf_ratios=bands,
      hf_change_db=change,
      recovery_db=recovery,
      transitions=rows,
      thresholds=dict(hf_change_max_db=hf_max,
                      control_variation_db=control_variation,
                      fade_margin_db=3.,
                      recovery_db=3.,
                      fade_dip_db=-6.,
                      crest_db=20.,
                      step_db=STEP_MAX_DB),
      limitation=
      'provisional regression thresholds; switch interval includes transport and guard time'
  )


def pitch_rule(audio, sr, scale, offset=0.):
  """Each isolated grain: resolve tone, then nearest scale degree within 15 cents."""
  notes = numpy.asarray(SCALES[scale - 0xa1][1])
  rows = []
  for trigger in TRIGGERS:
    # Ignore attack; avoid neighboring grains. Tone source is C4.
    a = round((trigger + offset + .035) * sr)
    b = round((trigger + offset + .20) * sr)
    part = numpy.asarray(audio[a:b], float)
    if part.ndim != 2 or len(part) != b - a or not numpy.isfinite(part).all():
      rows.append(dict(trigger_s=trigger, verdict='INCONCLUSIVE'))
      continue
    x = part[:, numpy.argmax(numpy.mean(part**2, axis=0))]
    x = x - x.mean()
    if numpy.sqrt(numpy.mean(x * x)) < .0001:
      rows.append(dict(trigger_s=trigger, verdict='INCONCLUSIVE'))
      continue
    n = 1 << (len(x) * 8 - 1).bit_length()
    power = abs(numpy.fft.rfft(x * numpy.hanning(len(x)), n))**2
    k = int(numpy.argmax(power[1:]) + 1)
    # Log-parabolic interpolation avoids bin rounding at low grain pitches.
    shift = 0.
    if 1 < k < len(power) - 1:
      left, center, right = numpy.log(numpy.maximum(power[k - 1:k + 2], 1e-30))
      denominator = left - 2 * center + right
      if denominator:
        shift = float(numpy.clip(.5 * (left - right) / denominator, -.5, .5))
    freq = (k + shift) * sr / n
    # Require a narrow dominant tone, not noise or an unresolved mixture.
    bandwidth = max(2, round(15 * n / sr))
    concentration = power[max(1, k - bandwidth):k + bandwidth +
                          1].sum() / power.sum()
    if not 40 < freq < 8000 or concentration < .9:
      rows.append(dict(trigger_s=trigger, verdict='INCONCLUSIVE'))
      continue
    semitones = 12 * numpy.log2(freq / 261.625565)
    error = float(numpy.min(abs((semitones - notes + 6) % 12 - 6)) * 100)
    rows.append(
        dict(trigger_s=trigger,
             hz=float(freq),
             cents_error=error,
             verdict='PASS' if error <= 15 else 'FAIL'))
  verdict = next((v for v in ('FAIL', 'INCONCLUSIVE') if any(r['verdict'] == v
                                                             for r in rows)),
                 'PASS')
  if verdict == 'PASS' and len(
      {round(12 * numpy.log2(r['hz'] / 261.625565), 1)
       for r in rows}) < 2:
    # A constant in-scale tone does not exercise random pitch.
    verdict = 'INCONCLUSIVE'
  return dict(rule='grain_pitch_scale',
              verdict=verdict,
              grains=rows,
              tolerance_cents=15,
              scale=scale)


def random_depths(row):
  return (-.6, 0., .015,
          .03) if row.get('random_control') == 'size' else (-.6, 0., .15, .3)


def measure(audio, sr, row):
  """Timecode-aligned diagnostic summaries; no random waveform equivalence."""
  x = numpy.asarray(audio, float)
  if x.ndim != 2 or not numpy.isfinite(x).all():
    raise ValueError('invalid audio')
  if 'assignable_cv_destination' in row:
    # Identical isolated input bursts in each of three DC5 depth blocks.
    energy = []
    tail = []
    for t in (3., 11., 19.):

      def rms(a, b):
        part = x[round(a * sr):round(b * sr)]
        return float(numpy.sqrt(numpy.mean(part**2))) if len(part) == round(
            (b - a) * sr) else 0.

      energy.append(rms(t, t + 1))
      tail.append(rms(t + 1, t + 5))
    destination = row['assignable_cv_destination']
    ratio_db = lambda a, b: db(max(a, SILENCE), max(b, SILENCE))
    effect = ratio_db(energy[-1], energy[0]) if destination == 1 else ratio_db(
        tail[-1], tail[0])
    return dict(
        rule='assign_cv_effect',
        destination=destination,
        verdict='MEASURE' if max(energy) > 1e-4 else 'INCONCLUSIVE',
        levels=(-.5, 0., .5),
        burst_rms=energy,
        tail_rms=tail,
        effect_db=effect,
        expected='dry/wet changes burst energy'
        if destination == 1 else 'increased ' +
        ('feedback recirculation' if destination == 0 else 'reverb tail'),
        limitation=
        'diagnostic energy/tail response; no calibrated absolute threshold')
  # Each SEED edge has a disjoint window. Estimate grain onset and duration
  # from 5 ms RMS bins, then estimate pitch on the audible support only.
  pitches = []
  onsets = []
  durations = []
  depths = []
  frequencies = []
  resolved_triggers = []
  for i, t in enumerate(TRIGGERS):
    part = x[round(t * sr):round((t + .8) * sr)]
    width = max(1, round(.005 * sr))
    count = len(part) // width
    if count < 10:
      continue
    envelope = numpy.sqrt(
        numpy.mean(part[:count * width].reshape(count, width, 2)**2,
                   axis=(1, 2)))
    active = numpy.flatnonzero(envelope > max(1e-4,
                                              float(envelope.max()) * .15))
    if len(active) < 4:
      continue
    a, b = int(active[0] * width), int((active[-1] + 1) * width)
    # A window filled to its edge cannot resolve a grain's end.
    if b >= len(part) - width:
      continue
    signal = part[a:b].mean(axis=1)
    signal -= signal.mean()
    n = 1 << (len(signal) * 8 - 1).bit_length()
    power = abs(numpy.fft.rfft(signal * numpy.hanning(len(signal)), n))**2
    k = int(numpy.argmax(power[1:]) + 1)
    frequency = k * sr / n
    band = max(2, round(20 * n / sr))
    if not 40 < frequency < 8000 or power[max(1, k - band):k + band +
                                          1].sum() < .9 * power.sum():
      continue
    resolved_triggers.append(t)
    frequencies.append(float(frequency))
    pitches.append(float(12 * numpy.log2(frequency / 261.625565)))
    onsets.append(a / sr)
    durations.append((b - a) / sr)
    depths.append(i // 6)
  if row.get('random_control') == 'time':
    # Input is a known 16 s 200..800 Hz chirp. Recover circular read age
    # from grain frequency; SIZE/SHAPE/PITCH random depths remain zero.
    onsets = [
        float(((t + 17. - (f - 200.) / 37.5) % 16.) - 8.)
        for t, f in zip(resolved_triggers, frequencies)
    ]

  def spread(v):
    return float(numpy.percentile(v, 90) -
                 numpy.percentile(v, 10)) if len(v) > 1 else 0.

  return dict(
      rule='random_side_statistics',
      verdict='MEASURE' if len(pitches) >= 16 and
      all(depths.count(i) >= 4 for i in range(4)) else 'INCONCLUSIVE',
      resolved_grains=len(pitches),
      grain_hz=frequencies,
      pitch_spread_st=spread(pitches),
      timing_spread_s=spread(onsets),
      size_spread_s=spread(durations),
      by_depth=[
          dict(depth=v,
               count=depths.count(i),
               pitch_spread_st=spread(
                   [p for p, d in zip(pitches, depths) if d == i]),
               timing_spread_s=spread(
                   [p for p, d in zip(onsets, depths) if d == i]),
               size_spread_s=spread(
                   [p for p, d in zip(durations, depths) if d == i]))
          for i, v in enumerate(random_depths(row))
      ],
      limitation=
      'TIME uses chirp-derived circular buffer age; SIZE uses envelope duration; 24 stochastic grains, 6 per depth'
  )


def statistical_compare(stock, dz):
  """Finite-sample diagnostic tolerance: 2x spread or absolute resolution floor."""
  a = next((r for r in stock.get('rules', [])
            if r.get('rule') == 'random_side_statistics'), None)
  b = next((r for r in dz.get('rules', [])
            if r.get('rule') == 'random_side_statistics'), None)
  if not a or not b or a.get('verdict') != 'MEASURE' or b.get(
      'verdict') != 'MEASURE':
    return dict(verdict='INCONCLUSIVE')
  metrics = {}
  pairs = [('', a, b)]
  for i, (left,
          right) in enumerate(zip(a.get('by_depth', []), b.get('by_depth',
                                                               []))):
    pairs.append((f'depth_{i}_', left, right))
  for prefix, left, right in pairs:
    for key, floor in (('pitch_spread_st', 1.), ('timing_spread_s', .025),
                       ('size_spread_s', .005)):
      x, y = left[key], right[key]
      lower = max(0., x * .5 - floor)
      upper = x + max(floor, abs(x))
      metrics[prefix + key] = dict(stock=x,
                                   dz=y,
                                   lower=lower,
                                   upper=upper,
                                   tolerance=max(floor, abs(x)),
                                   within=lower - 1e-12 <= y <= upper + 1e-12)
  return dict(
      verdict='MEASURE',
      within_tolerance=all(v['within'] for v in metrics.values()),
      metrics=metrics,
      limitation=
      'provisional finite-sample tolerance; no deterministic waveform verdict')


def scale_measurement(measurement, scale):
  notes = numpy.asarray(SCALES[scale - 0xa1][1])
  frequencies = measurement.get('grain_hz', [])
  errors = [
      float(
          numpy.min(abs((12 * numpy.log2(f / 261.625565) - notes + 6) % 12 - 6))
          * 100) for f in frequencies
  ]
  verdict = 'FAIL' if any(e > 15 for e in errors) else (
      'PASS' if len(errors) == len(TRIGGERS) and
      measurement.get('pitch_spread_st', 0) > .5 else 'INCONCLUSIVE')
  return dict(rule='grain_pitch_scale',
              scale=scale,
              verdict=verdict,
              tolerance_cents=15,
              grain_hz=frequencies,
              cents_error=errors)

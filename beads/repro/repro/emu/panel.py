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
"""A scenario's panel as the firmware's own inputs.

Two things carry the panel into the emulator: the settings bank (option
bytes, read back by the real `Settings::Init`) and the pot ADC words. Field
offsets and enumerator values come from the ELF's DWARF; what is written here
is the saved-data encoding (an option byte is the option tag plus the
enumerator) and the pot laws, which mirror cv_reader.h.
"""

import numpy

from repro.emu.elf import ElfError

# Saved-data format: option byte = tag + enumerator (0 = stock).
OPTION_TAG = 0xA0
DETUNE_STEPS = (0.0, 1.0, 3.0, 4.0, 7.0, 12.0, 24.0, 24.0)

# Calibration the firmware writes for an uncalibrated unit, by CvAdcChannel
# name: scale, offset. The inverting front end gives a negative scale.
IDEAL_CAL = {
    "ASSIGN": (-2.0, 1.0),
    "DENSITY": (-2.0, 1.0),
    "PITCH": (-120.0, 60.0),
    "TIME": (-3.2, 1.6),
    "SIZE": (-3.2, 1.6),
    "SHAPE": (-3.2, 1.6)
}


def calibration_list(layout, by_name):
  """`by_name` as a list in CvAdcChannel order, from the DWARF."""
  enum = layout.enum("CvAdcChannel")
  out = [None] * enum["CV_ADC_CHANNEL_LAST"]
  for name, value in by_name.items():
    out[enum["CV_ADC_CHANNEL_" + name]] = value
  if None in out:
    raise ElfError(f"calibration lacks a CvAdcChannel: {out}")
  return out


SCALE_ENUM = {
    "chromatic": "PITCH_SCALE_CHROMATIC",
    "fifths": "PITCH_SCALE_FIFTHS",
    "major": "PITCH_SCALE_MAJOR",
    "minor": "PITCH_SCALE_MINOR",
    "major-pentatonic": "PITCH_SCALE_PENTATONIC_MAJOR",
    "minor-pentatonic": "PITCH_SCALE_PENTATONIC_MINOR",
    "pelog": "PITCH_SCALE_PELOG",
    "bhairav": "PITCH_SCALE_BHAIRAV",
    "shri": "PITCH_SCALE_SHRI"
}
LIMITER_ENUM = {
    "stock": "OUTPUT_LIMITER_STOCK",
    "fast": "OUTPUT_LIMITER_FAST",
    "off": "OUTPUT_LIMITER_OFF"
}
DELAY_ENUM = {
    "auto": "DELAY_MODE_AUTO",
    "never": "DELAY_MODE_NEVER",
    "wide_detent": "DELAY_MODE_WIDE_DETENT"
}
MIX_ENUM = {
    "feedback": "MIX_PARAMETER_FEEDBACK",
    "dry_wet": "MIX_PARAMETER_DRY_WET",
    "reverb": "MIX_PARAMETER_REVERB"
}
MEDIA_ENUM = {
    "dirty_tape": "MEDIA_DIRTY_TAPE",
    "clean_tape": "MEDIA_CLEAN_TAPE",
    "vintage_digital": "MEDIA_VINTAGE_DIGITAL",
    "digital": "MEDIA_DIGITAL"
}


def state_for(panel, layout):
  """The State fields a scenario panel asks for, over the firmware's own
    defaults for an uncalibrated unit (auto gain, ASSIGN on FEEDBACK)."""
  state = dict(
      media=layout.enum("Media")[MEDIA_ENUM[panel["media"]]],
      assignable_cv_destination=layout.enum("MixParameter")[MIX_ENUM[panel.get(
          "assign_destination", "feedback")]],
      assignable_cv_amount=(255, 0, 128),
      gain=0,
      gate_output_mode=0,
      frozen=0,
      disable_autosave=0,
      monophonic_mode=int(panel["mono"]))
  limiter = panel.get("limiter", "stock")
  if limiter != "stock":
    state["output_limiter_mode"] = OPTION_TAG + layout.enum(
        "OutputLimiterMode")[LIMITER_ENUM[limiter]]
  scale = panel.get("pitch_scale", "off")
  if scale != "off":
    state["pitch_scale"] = OPTION_TAG + layout.enum("PitchScale")[
        SCALE_ENUM[scale]]
  if panel.get("self_play") == "on_request":
    state["self_play"] = OPTION_TAG + 1
  if panel.get("reverb_style") == "clouds":
    state["reverb_style"] = OPTION_TAG + 2
  if panel.get("stereo_width", 0):
    state["stereo_width"] = int(panel["stereo_width"])
  delay = panel.get("delay_mode", "auto")
  if delay != "auto":
    state["delay_mode"] = OPTION_TAG + layout.enum("DelayMode")[
        DELAY_ENUM[delay]]
  if panel.get("width_assigned"):
    # Settings normalizes these two away otherwise.
    if not panel.get("stereo_width"):
      raise ValueError("width_assigned needs a stereo_width above 0")
    if panel.get("assign_destination", "feedback") != "feedback":
      raise ValueError("width_assigned replaces the ASSIGN destination; "
                       "leave assign_destination unset")
    state["width_assigned"] = 1
  return state


def media_value(layout, name):
  return layout.enum("Media")[MEDIA_ENUM[name]]


# ---------------------------------------------------------------- pots
def _normal(v):
  return int(min(65535, max(0, round(v * 65536.0))))


def _detune_law(raw):
  """cv_reader.h PotChannel LAW_DETUNE, vectorized (semitones)."""
  v = numpy.asarray(raw, dtype=numpy.float64) / 65536.0
  d = 2.0 * v - 1.0
  sign = numpy.where(d < 0, -1.0, 1.0)
  d = d * sign * 6.0
  integral = numpy.floor(d).astype(int)
  frac = d - integral
  frac = numpy.maximum((frac - 0.33333333) * 1.5, 0.0)
  smooth = frac * frac * (3.0 - 2.0 * frac)
  steps = numpy.asarray(DETUNE_STEPS)
  a = steps[integral]
  b = steps[numpy.minimum(integral + 1, 7)]
  return (a + (b - a) * smooth) * sign


_DETUNE_TABLE = None


def _detune(semitones):
  global _DETUNE_TABLE
  if _DETUNE_TABLE is None:
    _DETUNE_TABLE = _detune_law(numpy.arange(65536))
  err = numpy.abs(_DETUNE_TABLE - semitones)
  near = numpy.flatnonzero(err <= err.min() + 1e-9)
  # The law is flat around the centre detent; take the nearest to centre.
  return int(near[numpy.argmin(numpy.abs(near - 32768))])


def pot_words(panel, layout):
  """Twelve pot ADC words in PotsAdcChannel order. A panel value is the
    knob position, 0 fully counter-clockwise to 1 fully clockwise (the
    attenuverters too: 0.5 is their centre, 0 their full negative end, which
    is the randomization end of PITCH), except `pitch`, the detuning pot,
    which is the semitones it reports at that position."""
  enum = layout.enum("PotsAdcChannel")
  words = [32768] * enum["POTS_ADC_CHANNEL_LAST"]
  plain = {
      "FEEDBACK": "feedback",
      "DRY_WET": "dry_wet",
      "REVERB": "reverb",
      "TIME": "time",
      "DENSITY": "density",
      "SIZE": "size",
      "SHAPE": "shape"
  }
  for ch, key in plain.items():
    words[enum["POTS_ADC_CHANNEL_" + ch]] = _normal(panel[key])
  for ch in ("TIME", "SIZE", "SHAPE", "PITCH"):
    words[enum[f"POTS_ADC_CHANNEL_{ch}_AMOUNT"]] = _normal(panel[ch.lower() +
                                                                 "_att"])
  words[enum["POTS_ADC_CHANNEL_PITCH"]] = _detune(panel["pitch"])
  return words


# ---------------------------------------------------------------- CV
def cv_words(values, scale, offset):
  """ADC words for firmware-unit CV `values`: the inverse of
    `value = adc / 65536 * scale + offset`."""
  w = numpy.rint(
      (numpy.asarray(values, dtype=numpy.float64) - offset) / scale * 65536.0)
  return numpy.clip(w, 0, 65535).astype(numpy.int64)


def cv_index(layout, channel):
  name = "CV_ADC_CHANNEL_" + channel.upper()
  try:
    return layout.enum("CvAdcChannel")[name]
  except KeyError:
    raise ElfError(f"no CvAdcChannel {name}") from None

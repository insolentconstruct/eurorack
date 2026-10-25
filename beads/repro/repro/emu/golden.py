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
"""Fixed fixtures whose output hashes are recorded for a firmware build.

A fixture boots the firmware at one quality, configures it directly (the main
loop's mode monitor is bypassed so the mode stays fixed), settles 2000
blocks, and hashes the next 2000 blocks of codec output. A change in the
audio path moves a hash; a change that leaves the audio alone does not.

Modes: `grains` (stereo input), `mono` (right input unpatched), `delay`
(SIZE fully clockwise, settled transition), `synth` (wavetable synth, input
patched) and `selfplay` (synth, both inputs unpatched).
"""

import hashlib
import struct

import numpy

from repro.emu import firmware as F
from repro.emu import inputs as E
from repro.emu import machine as M

MODES = ("grains", "mono", "delay", "synth", "selfplay")
QUALITIES = (0, 1, 2, 3)
WARMUP, MEASURED, HASHED = 2000, 400, 2000
CONFIGURE = "_ZN5beads9Processor9ConfigureENS0_5MediaEb"
START_CODEC = "_Z22ConfigureAndStartCodecbb"
CODEC_TRANSITION = "codec_transition_frames"
# ADC words of the settled panel fixture, by PotsAdcChannel / CvAdcChannel
# enumerator names (looked up in the DWARF).
POT_WORDS = {
    "POTS_ADC_CHANNEL_FEEDBACK": 50000,
    "POTS_ADC_CHANNEL_DRY_WET": 65535,
    "POTS_ADC_CHANNEL_REVERB": 45000,
    "POTS_ADC_CHANNEL_TIME": 2048,
    "POTS_ADC_CHANNEL_DENSITY": 56000,
    "POTS_ADC_CHANNEL_PITCH": 39000,
    "POTS_ADC_CHANNEL_SIZE": 36000
}
POT_REST = 32768
# An unpatched CV jack reads the ADC word below/above the detector's
# threshold when the probe pin is high/low; jack -> CvAdcChannel enumerator.
CV_JACKS = {
    "cv1": "CV_ADC_CHANNEL_TIME",
    "cv2": "CV_ADC_CHANNEL_SIZE",
    "cv3": "CV_ADC_CHANNEL_SHAPE",
    "cv4": "CV_ADC_CHANNEL_PITCH",
    "cv5": "CV_ADC_CHANNEL_DENSITY"
}
CV_NORMALLED = (0xC000, 0x2000)
DELAY_RAMP = 1.5  # delay_mode_transition_ once settled


class Panel:
  """The fixture rig: a booted system, a two-sine input and fixed knobs."""

  def __init__(self, elf, quality, unpatched=(), itcm=b"", mode="grains"):
    self.fw = F.Firmware(elf, dict(media=quality), itcm=itcm)
    # Seed, the CV jacks and any listed audio input are unpatched.
    event = elf.layout().enum("Event")
    self.inputs = E.Inputs(
        unpatched=("seed", "cv1", "cv2", "cv3", "cv4", "cv5") +
        tuple(unpatched),
        event_bits=(event["EVENT_SEED"], event["EVENT_FREEZE"]))
    self.b = 0
    self.frame = 0
    enum = elf.layout().enum("PotsAdcChannel")
    cv_enum = elf.layout().enum("CvAdcChannel")
    self.cv_index = {
        j: (cv_enum[name], CV_NORMALLED)
        for j, name in CV_JACKS.items()
    }
    self.pots = [0, 0, 0] + [32768] * 9
    for name, word in POT_WORDS.items():
      self.pots[enum[name]] = word
    if mode == "delay":
      self.pots[enum["POTS_ADC_CHANNEL_SIZE"]] = 65535

  def step(self, count, main_pass=True):
    out = bytearray()
    fw = self.fw
    for _ in range(count):
      self.inputs.due(self.b)
      fw.set_inputs(self.inputs.idr_words())
      n = fw.block_size
      f = self.frame
      frames = [(int(.1 * 2147483648 * numpy.sin(2 * numpy.pi * (f + i) / 64)),
                 int(.07 * 2147483648 * numpy.sin(2 * numpy.pi * (f + i) / 37)))
                for i in range(n)]
      gates = [self.inputs.freeze_bit(f + i) for i in range(n)]
      frames, gates, cv = self.inputs.normalled(fw.pin, frames, gates,
                                                [32768] * 6, self.cv_index)
      fw.block_index = self.b
      out += fw.block(F.pack_rx(frames), bytes(gates), cv, self.pots, M.FZ,
                      self.b % 4 == 3)
      if main_pass and self.b % 4 == 3:
        fw.main_pass()
      self.frame += n
      self.b += 1
    return bytes(out)


def run_fixture(elf, quality, mode, itcm=b"", counter=None):
  """Returns (sha256 of the hashed codec output, the output bytes)."""
  unpatched = ("in_l", "in_r") if mode == "selfplay" else \
      ("in_r",) if mode == "mono" else ()
  p = Panel(elf, quality, unpatched, itcm, mode)
  fw = p.fw
  m = fw.m
  configure = elf.func(CONFIGURE)
  start = elf.func(START_CODEC)
  m.call(configure, [fw.obj["processor"], quality,
                     int(mode == "mono")],
         fpscr=M.FZ)
  m.call(start, [int(mode in ("synth", "selfplay")), 0], fpscr=M.FZ)
  # Keep the mode fixed; SysTick and the callback still run.
  fw.main_pass = lambda: None
  m.w32(elf.obj(CODEC_TRANSITION, 4), 0)
  p.step(WARMUP)
  if mode == "delay":
    a = fw.obj["processor"] + elf.layout().offset("Processor",
                                                  "delay_mode_transition_")
    m.write(a, struct.pack("<f", DELAY_RAMP))
  (counter or Panel.step)(p, MEASURED)  # 400 blocks that advance the state
  audio = p.step(HASHED)
  return hashlib.sha256(audio).hexdigest(), audio

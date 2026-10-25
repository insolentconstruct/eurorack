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
"""Emulator tier: the firmware's own ELF running in a Cortex-M7 emulator.

The audio callback, SysTick and the main loop run as on the module, fed the
scenario's audio, gates, CV and panel through the ADC buffers, the GPIO input
registers and the codec frames. Every address, field offset and enumerator
comes from the ELF (symbols and DWARF); nothing is hard-coded. The render
matches the firmware bit for bit for a given ELF; the analog path (codec gain,
noise floor, output polarity) is not modelled.

Each run boots the firmware at the scenario's media, plays a quiet pre-roll so
the codec transition and the jack detector settle, plays the lead-in of any
tone that was already running (`phase_s`), then the scenario. Only the scenario
part is returned.
"""

from dataclasses import dataclass
import os
from pathlib import Path
import struct

import numpy

from repro import stimulus
from repro.emu import elf as elf_mod
from repro.emu import firmware as F
from repro.emu import inputs as E
from repro.emu import itcm, panel, qualify
from repro.emu import machine as M
from repro.scenario import CV_CHANNELS

PREROLL_S = 1.5
SEED_TAP_S, SEED_TAP_MS = 0.2, 100
# The quality-switch rule's schedule: reverb style Clouds-like at 45 s, back
# to stock at 105 s. The value is the saved option byte.
STYLE_SCHEDULE = ((45.0, 0xA2), (105.0, 0))
STYLE_SCENARIOS = ("clouds_ab", )  # rules that need the schedule


@dataclass
class Render:
  audio: numpy.ndarray  # (n, 2) float32 at sr
  sr: int
  delay_mode: numpy.ndarray  # (blocks,) uint8, 1 while in delay mode
  info: dict


class EmulatorTier:
  name = "emulator"
  sr = 48000

  def __init__(self, elf=None, window=None):
    self.elf_path = elf or os.environ.get("BEADS_ELF")
    self.window = window
    self.qualification = {}
    self.elf_sha256 = ""
    self._elf = None
    self._itcm = None
    self._boots = {}

  # ------------------------------------------------------------ setup
  def prepare(self, build_dir):
    """Check the ELF and the instrument once, before any worker starts."""
    if not self.elf_path or not Path(self.elf_path).is_file():
      raise RuntimeError(
          "the emulator tier needs the firmware ELF: pass --elf PATH or "
          "set BEADS_ELF (an app.elf built with debug info)")
    elf = elf_mod.Elf(self.elf_path)
    self.elf_sha256 = elf.sha256
    self.qualification = qualify.qualify()
    return {
        "elf": str(Path(self.elf_path).resolve()),
        "elf_sha256": self.elf_sha256,
        "window": self.window,
        "qualification": self.qualification
    }

  def restore(self, state):
    self.elf_path = state["elf"]
    self.elf_sha256 = state["elf_sha256"]
    self.window = state["window"]
    self.qualification = state["qualification"]

  def provenance(self):
    return {
        "tier": self.name,
        "elf_sha256": self.elf_sha256,
        "preroll_s": PREROLL_S,
        "window": self.window,
        "unicorn": self.qualification.get("unicorn"),
        "libunicorn_sha256": self.qualification.get("libunicorn_sha256")
    }

  def _load(self):
    if self._elf is None:
      self._elf = elf_mod.Elf(self.elf_path)
      if self._elf.sha256 != self.elf_sha256:
        raise RuntimeError("the ELF changed since prepare()")
      self._itcm = itcm.itcm_image(self._elf)
    return self._elf

  # ------------------------------------------------------------ boot
  def _boot(self, scn):
    elf = self._load()
    layout = elf.layout()
    state = panel.state_for(scn.panel, layout)
    fw = F.Firmware(elf,
                    state,
                    cal=panel.IDEAL_CAL,
                    itcm=self._itcm,
                    autostart=True)
    return fw

  def sample_rate(self, scn):
    """The codec rate the firmware runs at for this scenario's media."""
    fw = self._boots.get(scn.name) or self._boot(scn)
    self._boots[scn.name] = fw
    return fw.codec[-1][1]

  # ------------------------------------------------------------ run
  def run(self, scn, stim, seed):
    fw = self._boots.pop(scn.name, None) or self._boot(scn)
    elf, layout = self._elf, self._elf.layout()
    rate, n = fw.codec[-1][1], fw.block_size
    if stim.sr != rate:
      raise RuntimeError(f"{scn.name}: stimulus at {stim.sr} Hz, "
                         f"codec at {rate} Hz")
    fw.seed_rng(seed)
    blocks = stim.frames // n
    frames = blocks * n

    # --- lead-in: pre-roll of silence, then any earlier tone
    history_s = stimulus.history_seconds(scn)
    pre_blocks = int(round(PREROLL_S * 4000))
    hist = stimulus.history(scn, rate, history_s)
    hist_blocks = len(hist) // n
    hist = hist[len(hist) - hist_blocks * n:]
    lead = pre_blocks + hist_blocks
    total = lead + blocks
    audio = numpy.zeros((total * n, 2), numpy.float32)
    audio[pre_blocks * n:lead * n] = hist
    audio[lead * n:] = stim.audio[:frames]
    q31 = numpy.clip(numpy.rint(audio.astype(numpy.float64) * 2147483648.0),
                     -2147483648, 2147483647).astype("<i4")

    # Codec events: one value per frame, SEED and FREEZE as the firmware
    # defines them (Codec::Event) and as wide as its enumeration.
    event = layout.enum("Event")
    width = {1: "u1", 2: "<u2", 4: "<u4"}[layout.enum_size("Event")]
    gates = numpy.zeros(total * n, width)
    gates[lead * n:] = (
        stim.gates["seed"][:frames].astype(width) * event["EVENT_SEED"]
        | stim.gates["freeze"][:frames].astype(width) * event["EVENT_FREEZE"])

    # CV: firmware units -> ADC words through the stored calibration.
    cal = self._calibration(fw)
    cv = numpy.zeros((total, 6), numpy.int64)
    order = [panel.cv_index(layout, c) for c in CV_CHANNELS]
    for ch, idx in zip(CV_CHANNELS, order):
      scale, offset = cal[idx]
      track = numpy.zeros(total * n)
      track[lead * n:] = stim.cv[ch][:frames]
      cv[:, idx] = panel.cv_words(track[::n], scale, offset)
    pots = panel.pot_words(scn.panel, layout)

    # --- panel events
    inputs = E.Inputs(unpatched=() if scn.panel.get("inputs_patched", True) else
                      ("in_l", "in_r"),
                      event_bits=(event["EVENT_SEED"], event["EVENT_FREEZE"]))
    tap = int(SEED_TAP_S * 4000)
    button = scn.panel["seed_button"]
    if button == "off":
      inputs.add([(tap, "tap", "seed", SEED_TAP_MS)])
    elif button == "on":
      inputs.add([(tap, "press", "seed", None)])
    switches = []
    if any(r["rule"] in STYLE_SCENARIOS for r in scn.rules):
      switches = [(lead + int(round(t * 4000)), v) for t, v in STYLE_SCHEDULE]
    style_addr = fw.obj["ui"] + layout.offset("Ui", "clouds_style_")
    unpatched_audio = bool({"in_l", "in_r"} & inputs.unpatched)

    out = bytearray()
    delay = numpy.zeros(blocks, numpy.uint8)
    stop = total if self.window is None else min(
        total, lead + int(round(self.window * 4000)))
    for b in range(stop):
      if inputs.due(b):
        fw.set_inputs(inputs.idr_words())
      while switches and b >= switches[0][0]:
        value = switches.pop(0)[1]
        fw.write_state_field("reverb_style", value)
        fw.m.write(style_addr, bytes([1 if value else 0]))
      lo = b * n
      rx = q31[lo:lo + n]
      if unpatched_audio:
        rx = rx.copy()
        amp = E.PROBE_AMP if fw.pin else -E.PROBE_AMP
        for c, jack in ((0, "in_l"), (1, "in_r")):
          if jack in inputs.unpatched:
            rx[:, c] = amp
      if b >= lead:
        delay[b - lead] = fw.in_delay_mode()
      fw.block_index = b
      tx = fw.block(rx.tobytes(), gates[lo:lo + n].tobytes(), cv[b].tolist(),
                    pots, M.FZ, b % 4 == 3)
      if b >= lead:
        out += tx
      if b % 4 == 3:
        fw.main_pass()
    done = stop - lead
    render = (numpy.frombuffer(bytes(out), "<i4").reshape(-1, 2) /
              2147483648.0).astype(numpy.float32)
    info = {
        "blocks": int(done),
        "lead_blocks": int(lead),
        "codec": [list(c[:3]) for c in fw.codec],
        "ctx": {}
    }
    if switches or any(r["rule"] in STYLE_SCENARIOS for r in scn.rules):
      info["ctx"] = dict(
          start_epoch=0.0,
          transitions=dict(switches=[
              dict(value=v, before_epoch=t, after_epoch=t + 0.005, rc=0)
              for t, v in STYLE_SCHEDULE
          ]))
    return Render(render, rate, delay[:done], info)

  def _calibration(self, fw):
    """(scale, offset) per CV channel as Settings::Init loaded them."""
    layout = fw.layout
    base = fw.obj["settings"] + layout.offset("Settings", "persistent_data_") \
        + layout.offset("PersistentData", "adc_calibration")
    stride = layout.size("AdcCalibrationData")
    fields = layout.members("AdcCalibrationData")
    out = []
    for i in range(6):
      raw = fw.m.read(base + i * stride, stride)
      out.append((struct.unpack_from("<f", raw, fields["scale"])[0],
                  struct.unpack_from("<f", raw, fields["offset"])[0]))
    return out

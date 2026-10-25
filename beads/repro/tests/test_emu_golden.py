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
"""Golden hashes: the emulator must reproduce the recorded output of the
fixed fixtures on a firmware build whose audio path they were taken from.

Opt-in: set BEADS_ELF to an app.elf (with debug info) built from the sources
at or after the Beta 1 release candidates. The test skips when the ELF is
absent, and when its hash is not one the table was recorded on.
"""

import concurrent.futures
import multiprocessing
import os
from pathlib import Path
import unittest

from tests import helpers  # noqa: F401  (sets sys.path)

TABLE = Path(__file__).resolve().parent / "data" / "emulator_golden.tsv"
# ELFs whose fixtures produced the table (alpha 2 and the Beta 1 candidates
# have equal output hashes; this is the one a CI build is compared against).
KNOWN_ELFS = {
    "088c9362e6635d1944306b564d1650696926e11d2e9ad3d2b44c6e1b3a336aa1",
}


def read_table():
  rows = {}
  for line in TABLE.read_text().splitlines():
    if line.startswith(("#", "quality")) or not line.strip():
      continue
    q, mode, digest = line.split()
    rows[(int(q), mode)] = digest
  return rows


def _fixture(args):
  from repro.emu import elf as elf_mod, golden, itcm
  path, quality, mode = args
  elf = elf_mod.Elf(path)
  digest, _ = golden.run_fixture(elf, quality, mode, itcm.itcm_image(elf))
  return quality, mode, digest


class Golden(unittest.TestCase):

  def test_table_lists_every_fixture_once(self):
    from repro.emu import golden
    self.assertEqual(
        sorted(read_table()),
        sorted((q, m) for q in golden.QUALITIES for m in golden.MODES))

  @unittest.skipUnless(os.environ.get("BEADS_ELF"), "BEADS_ELF not set")
  def test_fixtures_reproduce_the_recorded_hashes(self):
    import hashlib
    path = os.environ["BEADS_ELF"]
    sha = hashlib.sha256(Path(path).read_bytes()).hexdigest()
    if sha not in KNOWN_ELFS:
      self.skipTest(f"ELF {sha[:12]} is not one the table was "
                    "recorded on")
    want = read_table()
    jobs = [(path, q, m) for q, m in sorted(want)]
    ctx = multiprocessing.get_context("spawn")
    with concurrent.futures.ProcessPoolExecutor(min(len(jobs),
                                                    os.cpu_count() or 1),
                                                mp_context=ctx) as pool:
      got = {(q, m): d for q, m, d in pool.map(_fixture, jobs)}
    self.assertEqual(got, want)


OPTION_PANELS = [
    dict(limiter="fast"),
    dict(limiter="off"),
    dict(pitch_scale="shri"),
    dict(pitch_scale="fifths", assign_destination="reverb"),
    dict(self_play="on_request"),
    dict(reverb_style="clouds"),
    dict(delay_mode="never"),
    dict(delay_mode="wide_detent"),
    dict(stereo_width=100),
    dict(stereo_width=255, width_assigned=True),
    dict(mono=True, media="clean_tape"),
    dict(media="dirty_tape", assign_destination="dry_wet"),
    dict(limiter="fast",
         pitch_scale="bhairav",
         self_play="on_request",
         reverb_style="clouds",
         delay_mode="never",
         stereo_width=7,
         assign_destination="reverb"),
]


class Options(unittest.TestCase):

  @unittest.skipUnless(os.environ.get("BEADS_ELF"), "BEADS_ELF not set")
  def test_every_panel_option_is_read_back_by_the_firmware(self):
    """Settings::Init must load each encoded option as written (the boot
        checks the state bytes), and the media sets the codec rate."""
    from repro import scenario
    from repro.emu import elf as elf_mod, firmware, itcm, panel
    elf = elf_mod.Elf(os.environ["BEADS_ELF"])
    image = itcm.itcm_image(elf)
    rates = {
        "dirty_tape": 24000,
        "clean_tape": 48000,
        "vintage_digital": 32000,
        "digital": 48000
    }
    for opts in OPTION_PANELS:
      with self.subTest(**opts):
        p = dict(scenario.PANEL_DEFAULTS, **opts)
        fw = firmware.Firmware(elf,
                               panel.state_for(p, elf.layout()),
                               itcm=image,
                               cal=panel.IDEAL_CAL,
                               autostart=True)
        self.assertEqual(fw.codec[-1][1], rates[p["media"]])


if __name__ == "__main__":
  unittest.main()

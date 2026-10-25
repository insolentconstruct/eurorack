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
"""The merged results must not depend on the job count."""

import os
import unittest

from repro import canon, runner
from tests import helpers


class Determinism(unittest.TestCase):

  def test_jobs_1_and_4_are_byte_identical(self):
    serial, _ = helpers.run("serial", 1)
    parallel, _ = helpers.run("parallel", 4)
    for name in ("merged.json", "merged.tsv"):
      self.assertEqual((serial / name).read_bytes(),
                       (parallel / name).read_bytes(), name)
    # Everything but wall-clock timing, per scenario, byte for byte.
    for d in sorted(p for p in serial.iterdir() if p.is_dir()):
      for f in sorted(d.iterdir()):
        if f.name == "timing.json":
          continue
        self.assertEqual(f.read_bytes(),
                         (parallel / d.name / f.name).read_bytes(),
                         f"{d.name}/{f.name}")

  def test_merged_json_is_canonical(self):
    serial, doc = helpers.run("serial2", 1)
    text = (serial / "merged.json").read_text()
    self.assertEqual(text, canon.dumps(doc))
    self.assertNotIn("\n  ", text)  # one serializer, no indent


@unittest.skipUnless(os.environ.get("BEADS_ELF"), "BEADS_ELF not set")
class EmulatorDeterminism(unittest.TestCase):
  """The emulator tier too: one process or several, the same bytes."""

  def test_jobs_1_and_2_are_byte_identical(self):
    kw = dict(only={"b-seed-trigger", "b-density-gated"},
              tier_options=dict(elf=os.environ["BEADS_ELF"], window=1.0))
    outs = []
    for name, jobs in (("emu1", 1), ("emu2", 2)):
      base, _ = runner.run_set("p0", "emulator",
                               helpers.scratch() / name, jobs, **kw)
      outs.append(base)
    for name in ("merged.json", "merged.tsv"):
      self.assertEqual((outs[0] / name).read_bytes(),
                       (outs[1] / name).read_bytes(), name)
    for d in sorted(p for p in outs[0].iterdir() if p.is_dir()):
      for f in sorted(d.iterdir()):
        if f.name != "timing.json":
          self.assertEqual(f.read_bytes(),
                           (outs[1] / d.name / f.name).read_bytes(),
                           f"{d.name}/{f.name}")


if __name__ == "__main__":
  unittest.main()

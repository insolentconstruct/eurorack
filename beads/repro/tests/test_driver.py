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
"""The host driver itself: the SIZE-fully-clockwise delay-mode guard."""

import unittest

from tests import helpers  # noqa: F401  (sets sys.path)
from repro import runner, scenario, stimulus, tiers

SILENT_TEXT = """
name = "{name}"
seconds = 5
tiers = ["desktop"]
{guard}
[panel]
size = {size}
[stimulus]
generator = "sine"
freq = 220
amp = 0.1
[[rules]]
rule = "silent"
win = [0.0, 0.1]
"""


class Driver(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    cls.tier = tiers.get("desktop")
    cls.tier.prepare(helpers.scratch() / "driver-build")

  def render(self, size, guard):
    scn = scenario.parse(
        SILENT_TEXT.format(name="g",
                           size=size,
                           guard=f'[guard]\ndelay_mode = "{guard}"'))
    return scn, self.tier.run(scn, stimulus.build(scn, 48000, 1), 1)

  def test_assign_cv_is_refused_not_ignored(self):
    scn = scenario.parse(
        SILENT_TEXT.format(name="a", size=0.5, guard="") +
        '[[events]]\nkind="cv"\nchannel="assign"\nt=1\nlevel=0.5\n')
    with self.assertRaisesRegex(RuntimeError, "ASSIGN"):
      self.tier.run(scn, stimulus.build(scn, 48000, 1), 1)

  def test_noon_knob_stays_in_grain_mode(self):
    scn, render = self.render(0.5, "never")
    self.assertEqual(int(render.delay_mode.sum()), 0)
    self.assertEqual(runner._guard_result(scn, render)["verdict"], "PASS")

  def test_knob_fully_clockwise_enters_delay_mode(self):
    scn, render = self.render(1.0, "always")
    self.assertEqual(int(render.delay_mode[-1]), 1)
    self.assertEqual(runner._guard_result(scn, render)["verdict"], "PASS")
    # The same render under a "never" guard is what the guard exists to catch.
    scn.guard = "never"
    self.assertEqual(runner._guard_result(scn, render)["verdict"], "INVALID")


if __name__ == "__main__":
  unittest.main()

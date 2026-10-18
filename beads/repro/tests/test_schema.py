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


"""Scenario files: what loads, what is refused."""

import unittest

from tests import helpers  # noqa: F401  (sets sys.path)
from repro import scenario

BASE = """
name = "t"
seconds = 5
tiers = ["desktop"]
[stimulus]
generator = "sine"
freq = 220
amp = 0.1
[[rules]]
rule = "silent"
win = [0.0, 1.0]
"""


class Schema(unittest.TestCase):
    def test_minimal_scenario_loads_with_bench_panel(self):
        s = scenario.parse(BASE)
        self.assertEqual(s.panel["size"], 0.5)
        self.assertEqual(s.panel["dry_wet"], 1.0)
        self.assertEqual(s.guard, "never")

    def test_unknown_keys_are_refused(self):
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse("bogus = 1\n" + BASE)
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(BASE.replace("[stimulus]", "[panel]\nsizee = 1\n[stimulus]"))

    def test_unknown_tier_and_event_channel_are_refused(self):
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(BASE.replace('"desktop"', '"laptop"'))
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(BASE + '\n[[events]]\nkind="cv"\nchannel="seed"\nt=1\nlevel=1\n')

    def test_event_times_must_lie_inside_the_scenario(self):
        for bad in ("-1", "6", "inf"):
            ev = f'\n[[events]]\nkind="trigger"\nchannel="seed"\ntimes=[{bad}]\n'
            with self.assertRaises(scenario.ScenarioError):
                scenario.parse(BASE + ev)

    def test_names_are_one_safe_path_component(self):
        for bad in ("../x", "/abs", "a/b", ".hidden", "a b"):
            with self.assertRaises(scenario.ScenarioError):
                scenario.parse(BASE.replace('name = "t"', f'name = "{bad}"'))

    def test_size_fully_clockwise_needs_an_explicit_delay_guard(self):
        cw = BASE.replace("[stimulus]", "[panel]\nsize = 1.0\n[stimulus]")
        with self.assertRaisesRegex(scenario.ScenarioError, "delay mode"):
            scenario.parse(cw)
        ok = cw.replace("[panel]", '[guard]\ndelay_mode = "always"\n[panel]')
        self.assertEqual(scenario.parse(ok).guard, "always")

    def test_always_guard_needs_the_knob_up(self):
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(BASE.replace("[stimulus]",
                                        '[guard]\ndelay_mode = "always"\n[stimulus]'))

    def test_shipped_scenarios_load_and_size_cv_forbids_delay(self):
        names = [s.name for s in scenario.load_set(helpers.HERE, "p0")]
        self.assertEqual(names, ["b-seed-trigger", "b-density-gated", "b-size-cv"])
        size = scenario.load_set(helpers.HERE, "p0")[2]
        self.assertEqual((size.guard, size.panel["size"]), ("never", 0.5))


if __name__ == "__main__":
    unittest.main()

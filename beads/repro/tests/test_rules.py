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


"""Rules on synthetic renders, plus seeds, jobs and the delay-mode guard."""

import types
import unittest

import numpy as np

from tests import helpers  # noqa: F401  (sets sys.path)
from repro import jobs, rules, runner, scenario, stimulus

SR = 48000


def bursts(times, length=0.1, freq=440.0, total=6.0):
    x = np.zeros(int(total * SR))
    for t in times:
        k = np.arange(int(length * SR))
        x[int(t * SR):int(t * SR) + len(k)] = (
            0.3 * np.hanning(len(k)) * np.sin(2 * np.pi * freq * k / SR))
    return x


class Rules(unittest.TestCase):
    def test_onsets_match_triggers(self):
        x = bursts([1.0, 2.0, 3.5])
        r = rules.evaluate([dict(rule="onsets_match_triggers", win=[0.5, 5.0],
                                 triggers=[1.0, 2.0, 3.5])], x, SR)[0]
        self.assertEqual((r["verdict"], r["matched"], r["extra"]), ("PASS", 3, 0))
        r = rules.evaluate([dict(rule="onsets_match_triggers", win=[0.5, 5.0],
                                 triggers=[1.0, 2.0])], x, SR)[0]
        self.assertEqual(r["verdict"], "FAIL")

    def test_one_onset_cannot_answer_two_triggers(self):
        x = bursts([1.05, 2.0])
        r = rules.evaluate([dict(rule="onsets_match_triggers", win=[0.5, 3.0],
                                 triggers=[1.0, 1.02])], x, SR)[0]
        self.assertEqual(r["verdict"], "FAIL")

    def test_trigger_order_does_not_change_the_verdict(self):
        x = bursts([1.1, 1.25])
        got = [rules.evaluate([dict(rule="onsets_match_triggers", win=[0.5, 2.0],
                                    triggers=ts)], x, SR)[0]["verdict"]
               for ts in ([1.0, 1.08], [1.08, 1.0])]
        self.assertEqual(got[0], got[1])

    def test_opposite_channels_are_not_silence(self):
        x = np.stack([0.2 * np.ones(SR), -0.2 * np.ones(SR)], 1)
        r = rules.evaluate([dict(rule="silent", win=[0.1, 0.5])], x, SR)[0]
        self.assertEqual(r["verdict"], "FAIL")

    def test_silent_and_unknown_rule(self):
        x = bursts([1.0])
        res = rules.evaluate([dict(rule="silent", win=[3.0, 5.0]),
                              dict(rule="silent", win=[0.9, 1.2]),
                              dict(rule="nonsense")], x, SR)
        self.assertEqual([r["verdict"] for r in res], ["PASS", "FAIL", "INVALID"])

    def test_rate_increasing(self):
        x = bursts([0.5, 2.5, 3.2, 4.2, 4.6, 5.0, 6.0, 6.4, 6.8, 7.2, 7.6],
                   total=10.0)
        wins = [[0, 2], [2, 4], [4, 6], [6, 8]]
        r = rules.evaluate([dict(rule="rate_increasing", wins=wins)], x, SR)[0]
        self.assertEqual(r["verdict"], "PASS", r)
        # The last window must hold within 85% of the best earlier one.
        r = rules.evaluate([dict(rule="rate_increasing", wins=wins)],
                           bursts([0.5, 2.5, 3.2, 4.2, 4.6, 5.0], total=10.0),
                           SR)[0]
        self.assertEqual(r["verdict"], "FAIL")

    def test_direction_reads_chirp_slope(self):
        k = np.arange(int(0.8 * SR)) / SR
        up = 0.3 * np.sin(2 * np.pi * (300 * k + 600 * k * k))
        x = np.zeros(2 * SR)
        x[SR // 2:SR // 2 + len(up)] = up
        for expect, want in (("forward", "PASS"), ("reverse", "FAIL")):
            r = rules.evaluate([dict(rule="direction", win=[0.4, 1.6],
                                     expect=expect, triggers=[0.5])], x, SR)[0]
            self.assertEqual(r["verdict"], want)

    def test_overall_orders_by_severity(self):
        self.assertEqual(rules.overall(["PASS", "MEASURE"]), "MEASURE")
        self.assertEqual(rules.overall(["PASS", "FAIL", "MEASURE"]), "FAIL")
        self.assertEqual(rules.overall(["FAIL", "INVALID"]), "INVALID")


class Seeds(unittest.TestCase):
    def test_seed_depends_on_names_only(self):
        a = runner.scenario_seed("p0", "x", 0)
        self.assertEqual(a, runner.scenario_seed("p0", "x", 0))
        self.assertNotEqual(a, runner.scenario_seed("p0", "y", 0))
        self.assertNotEqual(a, runner.scenario_seed("p0", "x", 1))
        self.assertNotEqual(a, runner.scenario_seed("q", "x", 0))

    def test_noise_follows_the_seed(self):
        s = scenario.parse((helpers.HERE / "scenarios/behaviours/b-seed-trigger.toml").read_text()
                           .replace("amp = 0.12\n", "amp = 0.12\n\n[[stimulus.layers]]\n"
                                   "kind = \"noise\"\namp = 0.01\n", 1))
        one = stimulus.build(s, SR, 1).sha256()
        self.assertEqual(one, stimulus.build(s, SR, 1).sha256())
        self.assertNotEqual(one, stimulus.build(s, SR, 2).sha256())


class Jobs(unittest.TestCase):
    def test_precedence_cli_then_env_then_default(self):
        self.assertEqual(jobs.resolve(3, {"BEADS_JOBS": "7"}), 3)
        self.assertEqual(jobs.resolve(None, {"BEADS_JOBS": "7"}), 7)
        self.assertEqual(jobs.resolve(None, {"BEADS_JOBS": ""}),
                         min(4, jobs.usable_cores()))
        with self.assertRaises(SystemExit):
            jobs.resolve(None, {"BEADS_JOBS": "0"})

    def test_cgroup_quota_caps_the_default(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "cpu.max").write_text("200000 100000\n")
            self.assertEqual(jobs.cgroup_cpus(d), 2)
            self.assertLessEqual(jobs.usable_cores(d), 2)
            (Path(d) / "cpu.max").write_text("max 100000\n")
            self.assertIsNone(jobs.cgroup_cpus(d))


class Guard(unittest.TestCase):
    def fake(self, guard, flags):
        scn = types.SimpleNamespace(guard=guard)
        render = types.SimpleNamespace(delay_mode=np.array(flags, np.uint8))
        return runner._guard_result(scn, render)

    def test_never_fails_when_delay_mode_seen(self):
        self.assertEqual(self.fake("never", [0, 0, 0])["verdict"], "PASS")
        self.assertEqual(self.fake("never", [0, 1, 1])["verdict"], "INVALID")

    def test_always_needs_delay_mode_at_the_end(self):
        self.assertEqual(self.fake("always", [0, 1, 1])["verdict"], "PASS")
        self.assertEqual(self.fake("always", [0, 0, 0])["verdict"], "INVALID")


if __name__ == "__main__":
    unittest.main()

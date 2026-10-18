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



"""The shared rules and stimulus builders on synthetic input, and the shipped
scenario files."""

from collections import Counter
import unittest

import numpy as np

from tests import helpers  # noqa: F401  (sets sys.path)
from repro import beta_rules, rules, scenario, stimulus

SR = 48000
ROOT = helpers.HERE


def tone(freq, seconds, amp=0.3, sr=SR):
    k = np.arange(int(seconds * sr))
    return amp * np.sin(2 * np.pi * freq * k / sr)


def burst_train(times, freq=440.0, length=0.1, total=6.0):
    x = np.zeros(int(total * SR))
    for t in times:
        n = int(length * SR)
        x[int(t * SR):int(t * SR) + n] = (
            0.3 * np.hanning(n) * tone(freq, length, 1.0))
    return x


def one(rule, x, sr=SR, **params):
    return rules.evaluate([dict(rule=rule, **params)], x, sr)[0]


class Level(unittest.TestCase):
    def test_not_silent(self):
        x = tone(440, 2.0)
        self.assertEqual(one("not_silent", x, win=[0.5, 1.5])["verdict"], "PASS")
        self.assertEqual(one("not_silent", np.zeros(SR * 2),
                             win=[0.5, 1.5])["verdict"], "FAIL")

    def test_dry_passes_and_band_ratio(self):
        x = tone(440, 2.0)
        self.assertEqual(one("dry_passes", x, win=[0.2, 1.8],
                             freq=440.0)["verdict"], "PASS")
        self.assertEqual(one("dry_passes", x, win=[0.2, 1.8],
                             freq=500.0)["verdict"], "FAIL")
        mixed = tone(220, 2.0) + 0.01 * tone(330, 2.0)
        r = one("band_ratio", mixed, win=[0.2, 1.8], present=[220.0],
                absent=[330.0], min_db=10.0)
        self.assertEqual(r["verdict"], "PASS")
        r = one("band_ratio", mixed, win=[0.2, 1.8], present=[330.0],
                absent=[220.0], min_db=10.0)
        self.assertEqual(r["verdict"], "FAIL")

    def test_level_step_is_a_measurement(self):
        x = np.concatenate([tone(440, 1.0, 0.02), tone(440, 1.0, 0.1)])
        r = one("level_step", x, quiet=[0.1, 0.9], loud=[1.1, 1.9], step_db=20.0)
        self.assertEqual(r["verdict"], "MEASURE")
        self.assertAlmostEqual(r["step_db"], 20 * np.log10(5), delta=0.2)

    def test_offset_shifts_every_window(self):
        x = np.concatenate([np.zeros(SR), tone(440, 1.0)])
        self.assertEqual(one("not_silent", x, win=[0.2, 0.8])["verdict"], "FAIL")
        r = rules.evaluate([dict(rule="not_silent", win=[0.2, 0.8])], x, SR,
                           offset_s=1.0)[0]
        self.assertEqual(r["verdict"], "PASS")


class Degenerate(unittest.TestCase):
    """Inputs that would otherwise pass, hang or crash without measuring."""

    def test_silent_window_cannot_pass_dry_passes(self):
        r = one("dry_passes", np.zeros(SR * 2), win=[0.2, 1.8], freq=60.0)
        self.assertEqual(r["verdict"], "FAIL")

    def test_a_single_window_is_not_an_ordering(self):
        for rule, extra in (("duration_monotonic", {}),
                            ("age_increasing", dict(tones=[200.0, 300.0], step_s=0.25))):
            r = one(rule, np.zeros(SR * 2), wins=[[0.0, 1.0]], **extra)
            self.assertEqual(r["verdict"], "ERROR", rule)

    def test_steps_shorter_than_a_frame_are_refused(self):
        with self.assertRaises(ValueError):
            stimulus.steps(SR, SR, 1e-9, [440.0], 0.1)
        text = (ROOT / "scenarios/behaviours/b-freeze-gate.toml").read_text()
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(text.replace("seg_s = 1.0", "seg_s = 0", 1))

    def test_stereo_noise_cannot_be_routed_to_one_channel(self):
        text = (ROOT / "scenarios/beta/beta-clouds-ab.toml").read_text()
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(text.replace("stereo = true", "stereo = true\nchannel = 0"))

    def test_noise_past_the_block_boundary_is_clipped(self):
        text = (ROOT / "scenarios/beta/beta-clouds-ab.toml").read_text()
        s = scenario.parse(text)
        s.seconds, s.events = 1.0001, []
        s.generator["layers"][0]["stop"] = 1.0001
        self.assertEqual(stimulus.build(s, SR, 1).frames, 48000)


class Pitch(unittest.TestCase):
    def grain_stream(self, freq, total=4.0):
        return burst_train([0.5 + 0.5 * i for i in range(6)], freq, 0.3, total)

    def test_pitch_ratio(self):
        x = np.concatenate([self.grain_stream(220.0), self.grain_stream(440.0)])
        win = dict(base=[0.4, 3.6], test=[4.4, 7.6], ratio=2.0, tol=0.03)
        self.assertEqual(one("pitch_ratio", x, **win)["verdict"], "PASS")
        win["ratio"] = 4.0
        self.assertEqual(one("pitch_ratio", x, **win)["verdict"], "FAIL")

    def test_pitch_shift_is_a_measurement(self):
        x = np.concatenate([tone(220, 2.0), tone(330, 2.0)])
        r = one("pitch_shift", x, base=[0.2, 1.8], test=[2.2, 3.8])
        self.assertEqual(r["verdict"], "MEASURE")
        self.assertAlmostEqual(r["ratio"], 1.5, delta=0.02)


class Rates(unittest.TestCase):
    def test_rate_order_and_divided(self):
        x = burst_train([0.2, 0.7, 1.2, 1.7, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2,
                         3.4, 3.6, 3.8, 4.0, 4.2, 4.4], total=5.0)
        r = one("rate_order", x, wins=[[0.0, 2.0], [2.0, 4.5]], clock_hz=8.0)
        self.assertEqual(r["verdict"], "PASS")
        r = one("rate_divided", x, wins=[[0.0, 2.0], [2.0, 4.5]], clock_hz=8.0)
        self.assertEqual(r["verdict"], "PASS")
        self.assertEqual(len(r["clock_over_rate"]), 2)

    def test_triggers_with_a_timing_guard_accept_an_early_onset(self):
        x = burst_train([0.98, 2.0], total=4.0)
        plain = one("onsets_match_triggers", x, win=[0.5, 3.0],
                    triggers=[1.0, 2.0])
        guarded = one("onsets_match_triggers", x, win=[0.5, 3.0],
                      triggers=[1.0, 2.0], timing_guard_s=0.05)
        self.assertEqual((plain["verdict"], guarded["verdict"]), ("FAIL", "PASS"))


class Delay(unittest.TestCase):
    def test_loop_period_and_inconclusive_when_silent(self):
        x = burst_train([1.0 + 0.3 * i for i in range(12)], total=6.0)
        r = one("loop_period", x, win=[1.0, 4.9], period=0.3, tol=0.03)
        self.assertEqual(r["verdict"], "PASS", r)
        r = one("loop_period", np.zeros(SR * 6), win=[1.0, 4.9], period=0.3,
                tol=0.03)
        self.assertEqual(r["verdict"], "INCONCLUSIVE")

    def test_echo_peak_ratio_is_a_measurement(self):
        x = burst_train([5.0, 8.0], length=0.01, total=12.0)
        r = one("echo_peak_ratio", x, bursts_a=[2.0, 5.0], bursts_b=[2.0, 5.0])
        self.assertEqual(r["verdict"], "MEASURE")
        self.assertAlmostEqual(r["ratio"], 1.0)

    def test_noise_floor_gates_level_dependent_rules(self):
        x = burst_train([1.0 + 0.3 * i for i in range(12)], total=6.0)
        entry = dict(rule="loop_period", win=[1.0, 4.9], period=0.3, tol=0.03)
        quiet = rules.evaluate([entry], x, SR, ctx={"noise_dbfs": [-100, -100]})[0]
        self.assertEqual(quiet["verdict"], "PASS")
        self.assertIn("level_margin_db", quiet)
        loud = rules.evaluate([entry], x, SR, ctx={"noise_dbfs": [-20, -20]})[0]
        self.assertEqual(loud["verdict"], "INCONCLUSIVE")
        missing = rules.evaluate([entry], x, SR, ctx={"noise_dbfs": None})[0]
        self.assertEqual(missing["verdict"], "INCONCLUSIVE")
        # No noise key at all: a digital render has no floor to protect.
        self.assertEqual(rules.evaluate([entry], x, SR)[0]["verdict"], "PASS")

    def test_clipped_windows_are_invalid(self):
        x = np.clip(tone(440, 2.0, 2.0), -1, 1)
        r = rules.evaluate([dict(rule="not_silent", win=[0.2, 1.8])], x, SR,
                           ctx={"clip_guard": True})[0]
        self.assertEqual(r["verdict"], "INVALID")


class Age(unittest.TestCase):
    TONES = stimulus.tones(dict(start=200.0, per_octave=24, count=32))

    def test_tone_sets_may_be_written_as_a_formula(self):
        self.assertEqual(len(self.TONES), 32)
        self.assertEqual(self.TONES[24], 400.0)

    def test_capacity_plateau_is_not_a_failure(self):
        n = int(18 * SR)
        x = stimulus.steps(n, SR, 0.25, self.TONES, 0.3)
        # Grains replaying the input from a fixed age of 1 s, 4 s, 4 s.
        out = np.zeros(n)
        for lo, hi, age in ((2.5, 6.9, 1.0), (7.5, 11.9, 4.0), (12.5, 16.9, 4.0)):
            a, b = int(lo * SR), int(hi * SR)
            out[a:b] = x[a - int(age * SR):b - int(age * SR)]
        p = dict(rule="age_increasing", wins=[[2.5, 6.9], [7.5, 11.9], [12.5, 16.9]],
                 tones=dict(start=200.0, per_octave=24, count=32), step_s=0.25)
        self.assertEqual(rules.evaluate([p], out, SR)[0]["verdict"], "FAIL")
        r = rules.evaluate([dict(p, capacity_s=4.0)], out, SR)[0]
        self.assertEqual(r["verdict"], "PASS", r)


class Lead(unittest.TestCase):
    def test_noise_threshold_uses_the_lead_in(self):
        x = np.zeros((SR * 2, 2))
        x += 1e-5 * np.random.default_rng(1).standard_normal(x.shape)
        floor = rules.noise_threshold(x, SR, 1.0)
        self.assertTrue(np.all(floor < -80))

    def test_noise_threshold_refuses_a_loud_or_missing_lead_in(self):
        loud = 0.1 * np.ones((SR * 2, 2))
        with self.assertRaisesRegex(ValueError, "not quiet"):
            rules.noise_threshold(loud, SR, 1.0)
        with self.assertRaisesRegex(ValueError, "missing quiet lead-in"):
            rules.noise_threshold(np.zeros((SR * 2, 2)), SR, 0.1)

    def test_self_play_has_no_noise_floor(self):
        loud = 0.1 * np.ones((SR * 2, 2))
        self.assertIsNone(rules.noise_for_scenario(loud, SR, 0.1, "self-play"))
        with self.assertRaises(ValueError):
            rules.noise_for_scenario(loud, SR, 1.0, "b-pitch-cv")

    def test_relative_silence_follows_the_loudest_rule_window(self):
        x = np.stack([tone(440, 3.0, 0.3), tone(440, 3.0, 0.3)], 1)
        x[2 * SR:] *= 1e-4
        rule_set = [("dry_passes", [], dict(win=(0.2, 1.8))),
                    ("silent", [], dict(win=(2.2, 2.9)))]
        seg_ = rules.seg(x, SR, (2.2, 2.9))
        r = rules.provisional_level_rule(seg_, "silent", x, SR, rule_set)
        self.assertEqual(r["verdict"], "PASS")
        with self.assertRaisesRegex(ValueError, "relative silence"):
            rules.silence_reference(x, SR, [("silent", [], dict(win=(0, 1)))])

    def test_guarded_rule_shrinks_stable_and_grows_onset_windows(self):
        stable = rules.guarded_rule(("not_silent", [], dict(win=(2.0, 4.0))), 0.25)
        self.assertEqual(stable[2]["win"], (2.25, 3.75))
        match = rules.guarded_rule(
            ("onsets_match_triggers", [], dict(win=(2.0, 4.0), triggers=[3.0])), 0.25)
        self.assertEqual((match[2]["win"], match[2]["timing_guard_s"]),
                         ((1.75, 4.25), 0.25))
        with self.assertRaises(ValueError):
            rules.guarded_rule(("not_silent", [], dict(win=(2.0, 2.4))), 0.25)

    def test_onset_count_near_a_boundary_is_inconclusive(self):
        x = burst_train([1.99, 3.0], total=5.0)
        r = rules.onset_count_rule(x, SR, dict(win=(2.0, 4.0), expect=1), 0.0,
                                   0.05, "onset_count")
        self.assertEqual(r["verdict"], "INCONCLUSIVE")
        r = rules.onset_count_rule(x, SR, dict(win=(2.0, 4.0), expect=1), 0.0,
                                   0.0, "onset_count")
        self.assertEqual(r["verdict"], "PASS")

    def test_session_verdict_rollup(self):
        self.assertEqual(rules.session_verdict(["PASS", "MEASURE"]), "MEASURE")
        self.assertEqual(rules.session_verdict(["MEASURE", "INCONCLUSIVE"]),
                         "INCONCLUSIVE")
        self.assertEqual(rules.session_verdict(["INCONCLUSIVE", "FAIL"]), "FAIL")
        self.assertEqual(rules.session_verdict([]), "PASS")


class Beta(unittest.TestCase):
    def test_crosstalk_needs_a_noise_floor_and_quiet_undriven_channel(self):
        x = np.zeros((SR * 20, 2))
        x[3 * SR:7 * SR, 0] = tone(997, 4.0, 0.1)
        x[11 * SR:15 * SR, 1] = tone(997, 4.0, 0.1)
        r = rules.evaluate([dict(rule="dry_crosstalk")], x, SR,
                           ctx={"noise_dbfs": [-100, -100]})[0]
        self.assertEqual(r["verdict"], "PASS", r)
        x[3 * SR:7 * SR, 1] = x[3 * SR:7 * SR, 0] * 0.1
        r = rules.evaluate([dict(rule="dry_crosstalk")], x, SR,
                           ctx={"noise_dbfs": [-100, -100]})[0]
        self.assertEqual(r["verdict"], "FAIL")
        r = rules.evaluate([dict(rule="dry_crosstalk")], x, SR)[0]
        self.assertEqual(r["verdict"], "INCONCLUSIVE")

    def test_scales_cover_the_documented_names(self):
        self.assertEqual([n for n, _ in beta_rules.SCALES][:3],
                         ["chromatic", "fifths", "major"])
        self.assertEqual(len(beta_rules.TRIGGERS), 24)

    def test_beta_rules_refuse_an_unaligned_render(self):
        r = rules.evaluate([dict(rule="random_side_statistics", control="pitch")],
                           np.zeros((SR, 2)), SR, offset_s=0.5)[0]
        self.assertEqual(r["verdict"], "ERROR")

    def test_random_side_with_no_grains_is_inconclusive(self):
        r = one("random_side_statistics", np.zeros((SR * 30, 2)), control="size")
        self.assertEqual(r["verdict"], "INCONCLUSIVE")


class Builders(unittest.TestCase):
    def test_sine_has_fades_and_bounds(self):
        x = stimulus.sine(SR, SR, 1000.0, 0.5, SR // 4, SR // 2)
        self.assertEqual(x[:SR // 4].any(), False)
        self.assertEqual(x[SR // 2:].any(), False)
        self.assertLessEqual(float(np.abs(x).max()), 0.5)
        self.assertLess(abs(x[SR // 4 + 1]), 0.01)  # fading in

    def test_steps_cycle_through_the_frequencies(self):
        x = stimulus.steps(4 * SR, SR, 1.0, [220.0, 330.0], 0.3)
        self.assertAlmostEqual(rules.dominant(x[:SR], SR), 220.0, delta=2)
        self.assertAlmostEqual(rules.dominant(x[SR:2 * SR], SR), 330.0, delta=2)
        self.assertAlmostEqual(rules.dominant(x[2 * SR:3 * SR], SR), 220.0, delta=2)

    def test_series_forms_agree(self):
        self.assertEqual(scenario.series(dict(start=2.0, step=0.5, count=3)),
                         [2.0, 2.5, 3.0])
        self.assertEqual(scenario.series([1, 2]), [1, 2])
        with self.assertRaises(scenario.ScenarioError):
            scenario.series(dict(start=2.0))

    def test_layers_are_stereo_and_channel_selectable(self):
        text = (ROOT / "scenarios/beta/beta-dry-crosstalk.toml").read_text()
        s = scenario.parse(text)
        st = stimulus.build(s, SR, 1)
        self.assertGreater(float(np.abs(st.audio[2 * SR:8 * SR, 0]).max()), 0.1)
        self.assertEqual(float(np.abs(st.audio[2 * SR:8 * SR, 1]).max()), 0.0)
        self.assertEqual(float(np.abs(st.audio[10 * SR:16 * SR, 0]).max()), 0.0)

    def test_noise_layer_follows_its_seed_and_the_scenario_seed(self):
        text = (ROOT / "scenarios/behaviours/u-delay.toml").read_text()
        s = scenario.parse(text)
        # The delay scenario fixes its noise seed: the scenario seed is moot.
        self.assertEqual(stimulus.build(s, SR, 1).sha256(),
                         stimulus.build(s, SR, 2).sha256())

    def test_bela_events_give_gate_pairs_and_ramps(self):
        s = scenario.load(ROOT / "scenarios/behaviours/b-density-gated.toml")
        ev = stimulus.bela_events(s, 44100)
        self.assertIn((88200, "seed", 0.6, 0), ev)
        self.assertIn((749700, "seed", 0.0, 0), ev)
        self.assertIn((132300, "density", 1.0, 529200), ev)

    def test_unknown_layer_kind_is_refused_by_the_schema(self):
        text = (ROOT / "scenarios/behaviours/cal-dry.toml").read_text()
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(text.replace('kind = "sine"', 'kind = "saw"'))

    def test_unknown_optional_panel_value_is_refused(self):
        text = (ROOT / "scenarios/beta/beta-limiter-off.toml").read_text()
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(text.replace('limiter = "off"', 'limiter = "loud"'))


class Shipped(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.all = {}
        for path in sorted((ROOT / "scenarios").rglob("*.toml")):
            s = scenario.load(path)
            cls.all[s.name] = s

    def test_every_set_loads(self):
        import tomllib
        sets = tomllib.loads((ROOT / "sets.toml").read_text())["sets"]
        for name in sets:
            scenario.load_set(ROOT, name)

    def test_every_scenario_is_in_a_set_and_every_rule_exists(self):
        import tomllib
        sets = tomllib.loads((ROOT / "sets.toml").read_text())["sets"]
        listed = {n for spec in sets.values() for n in spec["scenarios"]}
        self.assertEqual(listed, set(self.all))
        for s in self.all.values():
            for r in s.rules:
                self.assertIn(r["rule"], rules.RULES, s.name)
            # The scenario must build a stimulus at both rates a tier uses.
            for sr in (44100, 48000):
                st = stimulus.build(s, sr, 1)
                self.assertEqual(st.frames % stimulus.BLOCK, 0)
                self.assertTrue(np.isfinite(st.audio).all(), s.name)

    def test_counts_by_tier(self):
        counts = Counter(t for s in self.all.values() for t in s.tiers)
        self.assertEqual(len(self.all), 43)
        self.assertEqual(counts["desktop"], 7)
        self.assertEqual(counts["emulator"], 37)
        self.assertEqual(counts["module"], 43)
        self.assertEqual(counts["seed3"], 0)

    def test_desktop_scenarios_use_only_what_the_driver_has(self):
        for s in self.all.values():
            if "desktop" in s.tiers:
                self.assertFalse(set(s.panel) & set(scenario.OPTIONAL_PANEL), s.name)
                self.assertFalse(any(e["kind"] == "cv" and e["channel"] == "assign"
                                     for e in s.events), s.name)

    def test_delay_scenarios_say_so(self):
        for name in ("u-delay", "beta-media0-delay", "beta-media1-delay",
                     "beta-media2-delay"):
            self.assertEqual(self.all[name].guard, "always", name)
        self.assertEqual(self.all["b-size-cv"].guard, "never")


if __name__ == "__main__":
    unittest.main()

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

"""Emulator pieces that need no firmware: panel laws, CV words, history,
sharding, and the schema for the new panel options."""

import unittest

import numpy as np

from tests import helpers  # noqa: F401  (sets sys.path)
from repro import runner, scenario, stimulus
from repro.emu import inputs, panel


class FakeLayout:
    """The enumerations the panel mapping reads, as the DWARF gives them."""

    ENUMS = {
        "PotsAdcChannel": {n: i for i, n in enumerate(
            ["FEEDBACK", "DRY_WET", "REVERB", "TIME_AMOUNT", "SIZE_AMOUNT",
             "SHAPE_AMOUNT", "PITCH_AMOUNT", "TIME", "DENSITY", "PITCH",
             "SIZE", "SHAPE", "LAST"])},
        "CvAdcChannel": {n: i for i, n in enumerate(
            ["ASSIGN", "DENSITY", "PITCH", "TIME", "SIZE", "SHAPE"])},
        "Media": {"MEDIA_DIRTY_TAPE": 0, "MEDIA_CLEAN_TAPE": 1,
                  "MEDIA_VINTAGE_DIGITAL": 2, "MEDIA_DIGITAL": 3},
        "MixParameter": {"MIX_PARAMETER_FEEDBACK": 0, "MIX_PARAMETER_DRY_WET": 1,
                         "MIX_PARAMETER_REVERB": 2},
        "PitchScale": {"PITCH_SCALE_FIFTHS": 2},
        "OutputLimiterMode": {"OUTPUT_LIMITER_STOCK": 0,
                              "OUTPUT_LIMITER_FAST": 1,
                              "OUTPUT_LIMITER_OFF": 2},
        "DelayMode": {"DELAY_MODE_AUTO": 0, "DELAY_MODE_NEVER": 1,
                      "DELAY_MODE_WIDE_DETENT": 2},
    }

    def enum(self, name):
        e = dict(self.ENUMS[name])
        if name in ("PotsAdcChannel", "CvAdcChannel"):
            prefix = "POTS_ADC_CHANNEL_" if name[0] == "P" else "CV_ADC_CHANNEL_"
            e = {prefix + k: v for k, v in e.items()}
        return e


class PanelLaws(unittest.TestCase):
    def test_detune_zero_is_centre_and_monotone_in_sign(self):
        self.assertEqual(panel._detune(0.0), 32768)
        up, down = panel._detune(12.0), panel._detune(-12.0)
        self.assertGreater(up, 32768)
        self.assertLess(down, 32768)
        self.assertAlmostEqual(panel._detune_law([up])[0], 12.0, delta=0.5)

    def test_cv_words_invert_the_calibration(self):
        scale, offset = panel.IDEAL_CAL["PITCH"]  # +-60 semitones
        words = panel.cv_words([0.0, 12.0, -24.0], scale, offset)
        back = words / 65536.0 * scale + offset
        np.testing.assert_allclose(back, [0.0, 12.0, -24.0], atol=0.01)

    def test_pot_words_put_values_on_their_channels(self):
        s = scenario.load(helpers.HERE / "scenarios/behaviours/b-size-cv.toml")
        words = panel.pot_words(s.panel, FakeLayout())
        self.assertEqual(len(words), 12)
        self.assertEqual(words[8], 32768)   # DENSITY at noon
        self.assertEqual(words[10], 32768)  # SIZE at noon
        self.assertGreater(words[4], 65000)  # SIZE attenuverter fully CW (1.0)
        ccw = dict(s.panel, size_att=0.0, pitch_att=0.5)
        words = panel.pot_words(ccw, FakeLayout())
        self.assertEqual((words[4], words[6]), (0, 32768))

    def test_state_for_encodes_options_with_the_tag(self):
        p = dict(scenario.PANEL_DEFAULTS, limiter="fast", pitch_scale="fifths",
                 assign_destination="reverb", delay_mode="never",
                 stereo_width=17, self_play="on_request",
                 reverb_style="clouds")
        st = panel.state_for(p, FakeLayout())
        self.assertEqual(st["output_limiter_mode"], 0xA1)
        self.assertEqual(st["pitch_scale"], 0xA2)
        self.assertEqual(st["assignable_cv_destination"], 2)
        self.assertEqual(st["delay_mode"], 0xA1)
        self.assertEqual(st["stereo_width"], 17)
        wide = panel.state_for(dict(scenario.PANEL_DEFAULTS, stereo_width=9,
                                    width_assigned=True), FakeLayout())
        self.assertEqual((wide["width_assigned"], wide["assignable_cv_destination"]),
                         (1, 0))
        self.assertEqual((st["self_play"], st["reverb_style"]), (0xA1, 0xA2))
        stock = panel.state_for(scenario.PANEL_DEFAULTS, FakeLayout())
        self.assertNotIn("pitch_scale", stock)
        self.assertEqual(stock["gain"], 0)


class History(unittest.TestCase):
    def test_history_continues_the_scenario_tone(self):
        scn = scenario.load(helpers.HERE / "scenarios/beta/beta-scale-fifths.toml")
        sr = 48000
        self.assertEqual(stimulus.history_seconds(scn), 17.0)
        hist = stimulus.history(scn, sr, 17.0)
        body = stimulus.build(scn, sr, 1).audio
        both = np.concatenate([hist, body[:2000]])[:, 0].astype(np.float64)
        # One continuous sine across the join: the second difference of a
        # sine is -(w^2) times the sine, so the ratio is the same either side.
        d2 = both[2:] - 2 * both[1:-1] + both[:-2]
        w2 = (2 * np.pi * 261.625565 / sr) ** 2
        mid = len(hist)
        near = slice(mid - 20, mid + 20)
        np.testing.assert_allclose(d2[near], -w2 * both[1:-1][near], atol=2e-6)

    def test_no_history_without_a_phase(self):
        scn = scenario.load(helpers.HERE / "scenarios/behaviours/b-seed-trigger.toml")
        self.assertEqual(stimulus.history_seconds(scn), 0.0)


class Sharding(unittest.TestCase):
    def test_every_name_lands_in_exactly_one_shard(self):
        names = [f"s{i}" for i in range(60)]
        for k in (1, 3, 4):
            hits = [[n for n in names if runner.shard_of(n, k) == i]
                    for i in range(k)]
            self.assertEqual(sorted(sum(hits, [])), sorted(names))
        self.assertEqual(runner.shard_of("b-size-cv", 4),
                         runner.shard_of("b-size-cv", 4))


    def test_merged_shards_equal_one_run(self):
        import json
        import tempfile
        from pathlib import Path
        entry = lambda n, v: dict(scenario=n, verdict=v, seed=1,  # noqa: E731
                                  stimulus_sha256="0", guard=dict(verdict="PASS"),
                                  rules=[dict(rule="silent", behaviours=[],
                                              verdict=v)], observables={})
        entries = [entry("a", "PASS"), entry("b", "FAIL"), entry("c", "PASS")]
        whole, _ = runner.merge("s", "emulator", 0, entries)
        with tempfile.TemporaryDirectory() as tmp:
            roots = []
            for i, part in enumerate((entries[:1], entries[1:])):
                text, tsv = runner.merge("s", "emulator", 0, part)
                d = Path(tmp) / f"shard{i}" / "s" / "emulator"
                d.mkdir(parents=True)
                (d / "merged.json").write_text(text)
                roots.append(Path(tmp) / f"shard{i}")
            roots.append(Path(tmp) / "empty-shard")
            with self.assertRaises(SystemExit):  # a gap is an error
                runner.merge_shards("s", "emulator", Path(tmp) / "bad",
                                    roots[:1], expect=["a", "b", "c"])
            base, doc = runner.merge_shards("s", "emulator",
                                            Path(tmp) / "out", roots,
                                            expect=["a", "b", "c"])
            self.assertEqual((base / "merged.json").read_text(), whole)
            self.assertEqual(doc["verdict"], "FAIL")


    def test_default_completeness_check_reads_the_real_set(self):
        import tempfile
        from pathlib import Path
        names = [s.name for s in scenario.load_set(runner.ROOT, "behaviours")
                 if "emulator" in s.tiers]
        entry = lambda n: dict(scenario=n, verdict="PASS", seed=1,  # noqa: E731
                               stimulus_sha256="0", guard=dict(verdict="PASS"),
                               rules=[], observables={})
        with tempfile.TemporaryDirectory() as tmp:
            roots = []
            for i, part in enumerate((names[::2], names[1::2])):
                text, _ = runner.merge("behaviours", "emulator", 0,
                                       [entry(n) for n in part])
                d = Path(tmp) / f"shard{i}" / "behaviours" / "emulator"
                d.mkdir(parents=True)
                (d / "merged.json").write_text(text)
                roots.append(Path(tmp) / f"shard{i}")
            _, doc = runner.merge_shards("behaviours", "emulator",
                                         Path(tmp) / "out", roots)
            self.assertEqual(sorted(doc["scenarios"]), sorted(names))
            with self.assertRaises(SystemExit):
                runner.merge_shards("behaviours", "emulator",
                                    Path(tmp) / "bad", roots[:1])


class Events(unittest.TestCase):
    def test_tap_releases_later_and_gpio_follows(self):
        i = inputs.Inputs(events=[(10, "tap", "seed", 100)])
        self.assertFalse(i.due(9))
        self.assertTrue(i.due(10))
        word = i.idr_words()[inputs.GPIOA_IDR]
        self.assertEqual(word & (1 << 12), 0)
        self.assertTrue(i.due(10 + 100 * inputs.BLOCKS_PER_MS))
        self.assertEqual(i.idr_words()[inputs.GPIOA_IDR] & (1 << 12), 1 << 12)

    def test_unpatched_jack_follows_the_probe_pin(self):
        i = inputs.Inputs(unpatched=("in_l",))
        frames, _, _ = i.normalled(1, [(0, 5)], [0], [0] * 6)
        self.assertEqual(frames, [(inputs.PROBE_AMP, 5)])
        frames, _, _ = i.normalled(0, [(0, 5)], [0], [0] * 6)
        self.assertEqual(frames, [(-inputs.PROBE_AMP, 5)])


class Schema(unittest.TestCase):
    BASE = ('name = "t"\nseconds = 5\ntiers = ["emulator"]\n'
            '[stimulus]\ngenerator = "silence"\n[[rules]]\nrule = "silent"\n'
            'win = [0.0, 1.0]\n')

    def test_new_panel_options_load(self):
        s = scenario.parse(self.BASE.replace("[stimulus]", (
            '[panel]\nself_play = "on_request"\nreverb_style = "clouds"\n'
            'delay_mode = "wide_detent"\nwidth_assigned = true\n'
            'stereo_width = 200\n[stimulus]')))
        self.assertEqual(s.panel["stereo_width"], 200)

    def test_width_assigned_needs_a_width_and_the_default_destination(self):
        for bad in ('width_assigned = true',
                    'width_assigned = true\nstereo_width = 9\n'
                    'assign_destination = "reverb"'):
            with self.assertRaises(scenario.ScenarioError, msg=bad):
                scenario.parse(self.BASE.replace(
                    "[stimulus]", f"[panel]\n{bad}\n[stimulus]"))
        with self.assertRaises(ValueError):
            panel.state_for(dict(scenario.PANEL_DEFAULTS, width_assigned=True),
                            FakeLayout())

    def test_a_running_tone_cannot_fade_in(self):
        text = self.BASE.replace('generator = "silence"\n', (
            'generator = "layers"\n[[stimulus.layers]]\nkind = "sine"\n'
            'freq = 220\namp = 0.1\nphase_s = 3.0\n'))
        with self.assertRaises(scenario.ScenarioError):
            scenario.parse(text)
        scenario.parse(text.replace("phase_s = 3.0", "phase_s = 3.0\nfade_s = 0.0"))

    def test_bad_option_values_are_refused(self):
        for bad in ('delay_mode = "always"', "stereo_width = 300",
                    "stereo_width = true", 'self_play = "sometimes"'):
            with self.assertRaises(scenario.ScenarioError, msg=bad):
                scenario.parse(self.BASE.replace(
                    "[stimulus]", f"[panel]\n{bad}\n[stimulus]"))

    def test_latched_clock_outlasts_the_divider_window(self):
        s = scenario.load(helpers.HERE / "scenarios/behaviours/"
                          "u-latched-clocked.toml")
        last = max(max(scenario.series(e["times"]))
                   for e in s.events if e["kind"] == "trigger")
        window_end = max(w[1] for r in s.rules if r["rule"] == "rate_divided"
                         for w in r["wins"])
        self.assertGreater(last, window_end)
        self.assertLessEqual(last, s.seconds)


if __name__ == "__main__":
    unittest.main()

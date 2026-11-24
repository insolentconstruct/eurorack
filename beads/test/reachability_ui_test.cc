// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Panel transitions for generated synth and gesture assignments.

#define main ExistingOptionsMain
#include "beads/test/options_test.cc"
#undef main

void TestReachableSynthControls() {
  test_checker.Start("TestReachableSynthControls");
  size_t tested = 0;
  for (int state = PROFILE_OFF; state <= PROFILE_ADJUSTABLE; ++state) {
    for (int synth = SYNTH_BEADS; synth <= SYNTH_RINGS; ++synth) {
      for (int action = ACTION_SEED; action < ACTION_LAST; ++action) {
        uint8_t block[kProfileSize];
        for (size_t i = 0; i < kProfileSize; ++i) {
          block[i] = kProfileBlock[i];
        }
        const ProfileEntry entries[] = {
          { FEATURE_SYNTH, uint8_t(state), uint8_t(synth), 0, 0, 0 },
          { FEATURE_SYNTH_START, PROFILE_FIXED, 1, 0, 0, uint8_t(action) }
        };
        block[5] = 2;
        block[6] = sizeof(entries);
        block[7] = block[12] = block[13] = block[14] = block[15] = 0;
        memcpy(block + 16, entries, sizeof(entries));
        const size_t end = 16 + sizeof(entries);
        const uint32_t crc = Profile::Crc32(block, end);
        for (int i = 0; i < 4; ++i) {
          block[end + i] = crc >> (8 * i);
        }
        Rig r(false);
        r.settings.Init(false, block);
        const bool enabled = state == PROFILE_ADJUSTABLE ||
            (state == PROFILE_FIXED && synth != SYNTH_BEADS);
        if (enabled && action == ACTION_FREEZE) {
          CHECK(r.settings.profile_locked());
          continue;
        }
        CHECK(!r.settings.profile_locked());
        r.ui.Init(&r.settings, &r.gain, &r.calibration, &r.backup);
        r.ui.LoadOptions();
        r.Tick(40, true);
        r.ui.set_no_input(true);
        r.ui.set_synth_mode(false);
        CHECK(r.ui.DoAction(action));
        CHECK(r.ui.self_play_requested());
        if (!enabled) {
          continue;
        }
        const int first = state == PROFILE_FIXED ? synth : SYNTH_PLAITS;
        const int last = state == PROFILE_FIXED ? synth : SYNTH_RINGS;
        for (int playing = first; playing <= last; ++playing) {
          r.ui.set_synth_mode(true);
          r.ui.set_playing_synth(playing);
          const ProfileFeature feature = playing == SYNTH_PLAITS
              ? FEATURE_PLAITS_BANK : FEATURE_RINGS_POLYPHONY;
          const int values = kFeatureDefinitions[feature].values;
          const int from = r.settings.FeatureValue(feature);
          for (int step = 1; step <= values; ++step) {
            CHECK(r.ui.DoAction(ACTION_FREEZE));
            CHECK(r.settings.FeatureValue(feature) == (from + step) % values);
          }
          CHECK(r.settings.FeatureValue(feature) == from);
          ++tested;
        }
      }
    }
  }
  CHECK(tested == 16);
  printf("Panel bank/polyphony cycles: %zu\n", tested);
}

void TestFallbackReachability() {
  test_checker.Start("TestFallbackReachability");
  size_t tested = 0;
  for (int synth = PROFILE_OFF; synth <= PROFILE_ADJUSTABLE; ++synth) {
    for (int start = PROFILE_OFF; start <= PROFILE_ADJUSTABLE; ++start) {
      for (int value = 0; value < 2; ++value) {
        for (int page = 0; page < kNumPageSlots; ++page) {
          for (int action = ACTION_NONE; action < ACTION_LAST; ++action) {
            uint8_t block[kProfileSize];
            for (size_t i = 0; i < sizeof(block); ++i) {
              block[i] = kProfileBlock[i];
            }
            const ProfileEntry entries[] = {
              { FEATURE_SYNTH, uint8_t(synth), SYNTH_BEADS, 0, 0, 0 },
              { FEATURE_SYNTH_START, uint8_t(start), uint8_t(value),
                uint8_t(page), 0, uint8_t(action) }
            };
            block[5] = 2;
            block[6] = sizeof(entries);
            block[7] = block[12] = block[13] = block[14] = block[15] = 0;
            memcpy(block + 16, entries, sizeof(entries));
            const size_t end = 16 + sizeof(entries);
            const uint32_t crc = Profile::Crc32(block, end);
            for (int i = 0; i < 4; ++i) {
              block[end + i] = crc >> (8 * i);
            }
            Rig r(false);
            r.settings.Init(false, block);
            if (r.settings.profile_locked()) {
              continue;
            }
            r.ui.Init(&r.settings, &r.gain, &r.calibration, &r.backup);
            r.ui.LoadOptions();
            r.Tick(40, true);
            r.ui.set_no_input(true);
            const bool manual = r.settings.self_play_on_request();
            const State state = r.settings.state();
            uint8_t profile[sizeof(Profile)];
            memcpy(profile, &r.settings.profile(), sizeof(profile));
            for (int mask = 0; mask < 8; ++mask) {
              const SynthEngine playing = EffectiveSynth(SYNTH_BEADS, mask);
              r.ui.set_playable_synths(mask);
              r.ui.set_playing_synth(playing);
              CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
              CHECK(!memcmp(profile, &r.settings.profile(), sizeof(profile)));
              if (playing != SYNTH_PLAITS) {
                continue;
              }
              // Both stopped and running fallback retain a manual start/stop.
              for (int running = 0; running < 2; ++running) {
                r.ui.set_synth_mode(running);
                r.ui.ClearSelfPlayRequest();
                CHECK(r.ui.DoAction(ACTION_HOLD) == manual);
                CHECK(r.ui.self_play_requested() == manual);
              }
              r.ui.ClearSelfPlayRequest();
              const int from = r.settings.FeatureValue(FEATURE_PLAITS_BANK);
              for (int step = 1; step <= kNumPlaitsBanks; ++step) {
                CHECK(r.ui.DoAction(ACTION_FREEZE));
                CHECK(!r.ui.self_play_requested());
                CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) ==
                    (from + step) % kNumPlaitsBanks);
              }
              CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
              CHECK(!memcmp(profile, &r.settings.profile(), sizeof(profile)));
              ++tested;
            }
            // Repaired waves restore the profile's own start chord immediately.
            r.ui.set_playable_synths(SynthMask(true, true, true));
            r.ui.set_playing_synth(SYNTH_BEADS);
            r.ui.ClearSelfPlayRequest();
            if (manual) {
              CHECK(r.ui.DoAction(action));
              CHECK(r.ui.self_play_requested());
            }
            CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
            CHECK(!memcmp(profile, &r.settings.profile(), sizeof(profile)));
          }
        }
      }
    }
  }
  CHECK(tested > 0);
  printf("Fallback panel configurations: %zu\n", tested);
}

void TestFallbackHold() {
  test_checker.Start("TestFallbackHold");
  for (int manual = 0; manual < 2; ++manual) {
    for (int patched = 0; patched < 2; ++patched) {
      Rig r(false);
      uint8_t block[kProfileSize];
      CustomProfile(block, FEATURE_SYNTH_START, 0, 0, ACTION_FREEZE);
      ApplyCustomProfile(&r, block);
      r.settings.set_self_play_on_request(manual);
      r.ui.set_no_input(!patched);
      r.ui.set_playable_synths(SynthMask(false, true, false));
      r.ui.set_playing_synth(SYNTH_PLAITS);
      for (int running = 0; running < 2; ++running) {
        r.ui.set_synth_mode(running);
        r.ui.ClearSelfPlayRequest();
        r.settings.mutable_state()->gain = 123;
        const int restarts = r.gain.restarts;
        r.Down(SWITCH_MEDIA);
        r.Tick(1510, true);
        r.Up(SWITCH_MEDIA);
        const bool requested = manual && !patched;
        CHECK(r.ui.self_play_requested() == requested);
        CHECK(r.settings.state().gain == (requested ? 123 : 0));
        CHECK(r.gain.restarts == restarts + (requested ? 0 : 1));
      }
      // The bank chord consumes both releases without starting or stopping.
      r.ui.ClearSelfPlayRequest();
      const uint8_t media = r.settings.state().media;
      const int bank = r.settings.FeatureValue(FEATURE_PLAITS_BANK);
      const int chord_restarts = r.gain.restarts;
      for (int step = 1; step <= kNumPlaitsBanks; ++step) {
        r.Down(SWITCH_MEDIA);
        r.Down(SWITCH_FREEZE);
        r.Tick(1510, true);
        r.Up(SWITCH_FREEZE);
        r.Up(SWITCH_MEDIA);
        CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) ==
            (bank + step) % kNumPlaitsBanks);
        CHECK(!r.ui.self_play_requested());
        CHECK(!r.ui.trim_flag());
        CHECK(r.settings.state().media == media);
        CHECK(r.gain.restarts == chord_restarts);
      }
      // Adjusting manual gain consumes the hold instead of starting the synth.
      r.ui.ClearSelfPlayRequest();
      const int restarts = r.gain.restarts;
      r.Down(SWITCH_MEDIA);
      r.feedback = 0.1f;
      r.Tick(1700, true);
      CHECK(r.ui.assignable_cv_pot_[Ui::MIX_PARAMETER_FEEDBACK].
          editing_hidden_parameter());
      const uint8_t gain = r.settings.state().gain;
      r.Up(SWITCH_MEDIA);
      CHECK(r.settings.state().gain == gain);
      CHECK(!r.ui.self_play_requested());
      CHECK(r.gain.restarts == restarts);
      // Repaired content restores the free hold's automatic gain restart.
      r.ui.set_playable_synths(SynthMask(true, true, false));
      r.ui.set_playing_synth(SYNTH_BEADS);
      r.settings.mutable_state()->gain = 123;
      r.Down(SWITCH_MEDIA);
      r.Tick(1510, true);
      r.Up(SWITCH_MEDIA);
      CHECK(!r.ui.self_play_requested());
      CHECK(r.settings.state().gain == 0);
      CHECK(r.gain.restarts == restarts + 1);
    }
  }
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestReachableSynthControls();
  TestFallbackReachability();
  TestFallbackHold();
  return test_checker.failures() ? 1 : 0;
}

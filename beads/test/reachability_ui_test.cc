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

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestReachableSynthControls();
  return test_checker.failures() ? 1 : 0;
}

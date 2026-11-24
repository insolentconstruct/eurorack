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
// Generated profile placements and reversible panel controls.

#include <cstdio>
#include <cstring>

#include "beads/profile.h"
#include "beads/test/test_checker.h"

using namespace beads;

size_t tested = 0;
size_t accepted = 0;

bool Parse(const ProfileEntry* entries, size_t count, Profile* profile) {
  uint8_t block[kProfileSize];
  memset(block, 255, sizeof(block));
  for (int i = 0; i < 16; ++i) {
    block[i] = kProfileBlock[i];
  }
  block[5] = count;
  block[6] = count * sizeof(ProfileEntry);
  block[7] = block[12] = block[13] = block[14] = block[15] = 0;
  memcpy(block + 16, entries, count * sizeof(ProfileEntry));
  const size_t end = 16 + count * sizeof(ProfileEntry);
  const uint32_t crc = Profile::Crc32(block, end);
  for (int i = 0; i < 4; ++i) {
    block[end + i] = crc >> (8 * i);
  }
  ++tested;
  const bool valid = profile->Init(block);
  accepted += valid;
  return valid;
}

void CheckPanel(const Profile& profile) {
  const ProfileEntry& start = profile.entry(FEATURE_SYNTH_START);
  const ProfileEntry& synth = profile.entry(FEATURE_SYNTH);
  const bool manual = start.state == PROFILE_ADJUSTABLE ||
      (start.state == PROFILE_FIXED && start.value);
  CHECK(!manual || start.action != ACTION_NONE);
  CHECK(profile.Action(ACTION_HOLD) == FEATURE_NONE ||
      profile.Action(ACTION_HOLD) == FEATURE_SYNTH_START);
  // A synth that cannot play Plaits or Rings needs neither stepping chord.
  const bool banks = synth.state == PROFILE_ADJUSTABLE ||
      (synth.state == PROFILE_FIXED && synth.value);
  CHECK(!banks || profile.Action(ACTION_FREEZE) != FEATURE_SYNTH_START);
  for (int slot = 1; slot < kNumPageSlots; ++slot) {
    const ProfileFeature f = profile.Page(slot);
    CHECK(f == FEATURE_NONE || profile.adjustable(f));
  }
  for (int slot = 1; slot < AMOUNT_LAST; ++slot) {
    const ProfileFeature f = profile.Amount(slot);
    CHECK(f == FEATURE_NONE || profile.adjustable(f) ||
        f == FEATURE_GAIN_KNOB);
  }
}

void TestPlacements() {
  test_checker.Start("TestPlacements");
  // All byte values and simultaneous page, amount and action assignments.
  for (int id = 1; id < FEATURE_LAST; ++id) {
    const int values = kFeatureDefinitions[id].values;
    for (int state = PROFILE_OFF; state <= PROFILE_ADJUSTABLE; ++state) {
      for (int value = 0; value < (values ? values : 256); ++value) {
        for (int page = 0; page < kNumPageSlots; ++page) {
          for (int amount = 0; amount < AMOUNT_LAST; ++amount) {
            for (int action = 0; action < ACTION_LAST; ++action) {
              const ProfileEntry e = {
                uint8_t(id), uint8_t(state), uint8_t(value), uint8_t(page),
                uint8_t(amount), uint8_t(action)
              };
              Profile profile;
              if (Parse(&e, 1, &profile)) {
                CheckPanel(profile);
              }
            }
          }
        }
      }
    }
  }
}

void TestSynthStart() {
  test_checker.Start("TestSynthStart");
  for (int synth_state = 0; synth_state <= PROFILE_ADJUSTABLE; ++synth_state) {
    for (int synth = 0; synth < 3; ++synth) {
      for (int state = 0; state <= PROFILE_ADJUSTABLE; ++state) {
        for (int value = 0; value < 2; ++value) {
          for (int page = 0; page < kNumPageSlots; ++page) {
            for (int action = 0; action < ACTION_LAST; ++action) {
              const ProfileEntry e[] = {
                { FEATURE_SYNTH, uint8_t(synth_state), uint8_t(synth),
                  0, 0, 0 },
                { FEATURE_SYNTH_START, uint8_t(state), uint8_t(value),
                  uint8_t(page), 0, uint8_t(action) }
              };
              Profile profile;
              const bool ok = Parse(e, 2, &profile);
              const bool enabled = synth_state == PROFILE_ADJUSTABLE ||
                  (synth_state == PROFILE_FIXED && synth);
              if (enabled && action == ACTION_FREEZE) {
                CHECK(!ok);
              }
              if (ok) {
                CheckPanel(profile);
              }
            }
          }
        }
      }
    }
  }
}

int main() {
  TestPlacements();
  TestSynthStart();
  CHECK(accepted > 100);
  CHECK(accepted < tested);
  printf("Generated profiles: %zu accepted of %zu; failures: %d\n",
      accepted, tested, test_checker.failures());
  return test_checker.failures() ? 1 : 0;
}

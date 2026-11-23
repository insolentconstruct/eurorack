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
// Runs the released settings implementation, with its own flash stubs.

#include "beads/test/recovery_compat/runtime.h"
#define private public
#include "beads/settings.h"
#undef private
#include "compat_options.h"

using namespace beads;

int main(int argc, char** argv) {
  Require(argc == 5, "usage: DRIVER save|load IMAGE SCALE OFFSET");
  const bool save = !strcmp(argv[1], "save");
  Require(save || !strcmp(argv[1], "load"), "unknown mode");
  const float scale = strtof(argv[3], NULL);
  const float offset = strtof(argv[4], NULL);
  MapBank();
  Settings defaults;
  // A blank module can request the factory test while providing defaults.
  defaults.Init(false);
  PersistentData expected = defaults.persistent_data();
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    expected.adc_calibration[i].scale *= scale;
    expected.adc_calibration[i].offset *= offset;
  }
  if (save) {
    Settings& s = defaults;
    *s.mutable_persistent_data() = expected;
    s.SavePersistentData();
    State* state = s.mutable_state();
    state->media = 1;
    state->monophonic_mode = 1;
    state->gain = 37;
    state->frozen = 1;
    state->assignable_cv_amount[1] = 79;
    ChangeOptions(s);
    for (int i = 0; i < 5; ++i) {
      s.mutable_state()->gain = 33 + i;
      s.SaveState();
      SaveOptions(s, i);
    }
    memset(Bank(), 0x5a, 6 * 0x20000);
    Image(argv[2], true);
  } else {
    Image(argv[2], false);
    static uint8_t before[0x100000];
    memcpy(before, Bank(), sizeof(before));
    const uint32_t programs = Programs();
    const uint32_t erases = Erases();
    Settings s;
    Require(s.Init(false), "recovered Init");
    Require(Programs() == programs && Erases() == erases,
        "boot programmed or erased flash");
    Require(!memcmp(before, Bank(), sizeof(before)), "boot changed flash");
    CheckStorage(s);
    Require(!memcmp(s.persistent_data().adc_calibration,
        expected.adc_calibration, sizeof(expected.adc_calibration)),
        "calibration differs");
    State state = defaults.state();
    state.frozen = 0;
    Require(!memcmp(&s.state(), &state, 10), "stock state differs");
    CheckOptions(s, defaults);
  }
  return 0;
}

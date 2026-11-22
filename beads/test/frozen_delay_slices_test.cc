// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Frozen delay slices: a slow free-running clock and a very fast SEED clock.
// "./frozen_delay_slices_test hash" prints a hash of the taps for settings
// within the limits, to compare builds without sanitizers.
//
// g++ -DTEST -O1 -g -I. -fsanitize=address,undefined \
//   -fno-sanitize-recover=all -no-pie \
//   beads/test/frozen_delay_slices_test.cc beads/dsp/delay_clock.cc \
//   beads/dsp/delay_controller.cc beads/resources.cc stmlib/dsp/units.cc \
//   stmlib/utils/random.cc -o frozen_delay_slices_test && \
//   ./frozen_delay_slices_test

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "beads/dsp/delay_controller.h"

using namespace beads;
using namespace stmlib;

const int kMaxDelay = 192000;  // 16-bit stereo buffer.
const int kMinDelay = 64 + 4;

void Check(bool condition, const char* message) {
  if (!condition) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
}

uint32_t hash = 2166136261u;

void Hash(const void* data, size_t size) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  while (size--) {
    hash = (hash ^ *bytes++) * 16777619;
  }
}

// Freeze, then render; clock_period = 0 runs the free-running clock.
void Render(int clock_period, float density, float exponential_fm) {
  DelayController controller;
  controller.Init(48000.0f, kMaxDelay);
  RandomizedParameter position = { 0.5f, 0.0f, 0.0f, false };
  RandomizedParameter shape = { 0.5f, 0.0f, 0.0f, false };
  bool freeze = true;
  DelayController::Output out;
  for (int n = 0; n < 480000; ++n) {
    GateFlags gate = (clock_period && (n + 1) % clock_period == 0)
        ? GATE_FLAG_RISING | GATE_FLAG_HIGH
        : GATE_FLAG_LOW;
    controller.Render(
        clock_period ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_LOCKED,
        clock_period ? &gate : NULL,
        &freeze, density, 0.0f, exponential_fm, position, shape, &out, 1);
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
      Check(out.tap[i].delay >= 0 &&
          out.tap[i].delay <= (kMaxDelay + kMinDelay) << 10,
          "frozen tap outside the recording");
      Hash(&out.tap[i].delay, sizeof(out.tap[i].delay));
      Hash(&out.tap[i].gain, sizeof(out.tap[i].gain));
    }
    Hash(&out.gate, sizeof(out.gate));
  }
}

int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "hash")) {
    // Settings that never reach either limit, to compare builds.
    const float densities[] = { 0.1f, 0.3f, 0.45f, 0.55f, 0.7f, 0.9f };
    const int periods[] = { 0, 1200, 12000, 48000 };
    for (int p = 0; p < 4; ++p) {
      for (int d = 0; d < 6; ++d) {
        Render(periods[p], densities[d], p ? 0.0f : 0.2f);
      }
    }
    printf("hash %08x\n", hash);
    return 0;
  }
  // Pass "fast" to run only the second case.
  const bool fast_only = argc > 1 && !strcmp(argv[1], "fast");
  if (!fast_only) {
    // Slow free-running clock: 18 semitones of FM CV make it longer than the
    // buffer.
    Render(0, 0.5f, -0.3f);
    printf("PASS slow free-running clock\n");
  }
  // SEED clock every 4 samples, 1/16 ratio: a slice shorter than a sample.
  Render(4, 0.0f, 0.0f);
  printf("PASS fast SEED clock\n");
  return 0;
}

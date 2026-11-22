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
// Delay clock: more than 2^31 samples (about 12 h at 48 kHz) without a clock.
// "./delay_clock_wrap_test hash" prints a hash of the clock's outputs for
// shorter gaps, to compare builds without sanitizers.
//
// g++ -DTEST -O1 -g -I. -fsanitize=address,undefined \
//   -fno-sanitize-recover=all -no-pie \
//   beads/test/delay_clock_wrap_test.cc beads/dsp/delay_clock.cc \
//   -o delay_clock_wrap_test && ./delay_clock_wrap_test

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "beads/dsp/delay_clock.h"

using namespace beads;

void Check(bool condition, const char* message) {
  if (!condition) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
}

// Ticks, taps and gaps up to 10 s, to compare builds.
void PrintHash() {
  DelayClock clock;
  clock.Init(48000.0f);
  uint32_t hash = 2166136261u;
  const int periods[] = { 4800, 24000, 190000, 200000, 480000, 7000 };
  for (int p = 0; p < 6; ++p) {
    for (int n = 0; n < 8 * periods[p]; ++n) {
      clock.Process((n + 1) % periods[p] == 0, p == 5 && n % 9000 == 0);
      const float values[] = {
        clock.interval(0), clock.interval(1), clock.gain(0), clock.fade()
      };
      for (int i = 0; i < 4; ++i) {
        uint32_t bits;
        memcpy(&bits, &values[i], sizeof(bits));
        hash = (hash ^ bits) * 16777619;
      }
      hash = (hash ^ clock.retrig()) * 16777619;
    }
  }
  printf("hash %08x\n", hash);
}

int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "hash")) {
    PrintHash();
    return 0;
  }
  DelayClock clock;
  clock.Init(48000.0f);
  // A 120 BPM clock.
  for (int n = 0; n < 48000 * 10; ++n) {
    clock.Process(n % 24000 == 12000, false);
  }
  const float interval_0 = clock.interval(0);
  const float interval_1 = clock.interval(1);
  printf("intervals before the gap: %.1f %.1f\n", interval_0, interval_1);
  Check(fabsf(interval_0 - 24000.0f) < 48.0f ||
      fabsf(interval_1 - 24000.0f) < 48.0f, "120 BPM");

  // The clock stops for 2^31 samples, then ticks again: like any gap longer
  // than 4 s, the first tick only restarts the count.
  for (uint32_t n = 0; n < 0x80000000; ++n) {
    clock.Process(false, false);
  }
  for (int n = 0; n < 64; ++n) {
    clock.Process(n == 0, false);
  }
  printf("intervals after the gap: %.1f %.1f\n",
      clock.interval(0), clock.interval(1));
  Check(clock.interval(0) == interval_0 && clock.interval(1) == interval_1,
      "tempo changed by the first tick after the gap");
  printf("PASS idle for 2^31 samples\n");
  return 0;
}

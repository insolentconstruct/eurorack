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
// Wavetable guard row: a grain on the last wave of the last bank.
//
// g++ -DTEST -O1 -g -I. -fsanitize=address,undefined \
//   -fno-sanitize-recover=all -no-pie \
//   beads/test/wavetable_guard_row_test.cc beads/resources.cc \
//   stmlib/utils/random.cc -o wavetable_guard_row_test && \
//   ./wavetable_guard_row_test
//
// "./wavetable_guard_row_test hash" prints a hash of every bank's grains, to
// compare builds without sanitizers.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "beads/dsp/frame.h"
#include "beads/resources.h"
#include "beads/dsp/wavetable_grain.h"

using namespace beads;

const int kNumBanks = 12;
const int kRowSize = kTableSize + 4;

void Check(bool condition, const char* message) {
  if (!condition) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
}

uint32_t RenderGrain(int bank, float wave, uint32_t hash) {
  WavetableGrain grain;
  grain.Init(48000.0f);
  grain.Start(bank, 16, wave, 0.00371f, 4096.0f, 0.5f, 0.5f, 0.5f, 0.37f);
  for (int n = 0; n < 4096; ++n) {
    FloatFrame out;
    out.l = 0.0f;
    out.r = 0.0f;
    grain.Render(&out);
    uint32_t bits;
    memcpy(&bits, &out.l, sizeof(bits));
    hash = (hash ^ bits) * 16777619;
  }
  return hash;
}

int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "hash")) {
    uint32_t hash = 2166136261u;
    const float waves[] = { 0.0f, 0.3f, 0.5f, 0.999f, 1.0f };
    for (int bank = 0; bank < kNumBanks; ++bank) {
      for (int i = 0; i < 5; ++i) {
        hash = RenderGrain(bank, waves[i], hash);
      }
    }
    printf("hash %08x\n", hash);
    return 0;
  }
  // Wave 1.0 reads its row, and the next one with a zero weight.
  RenderGrain(kNumBanks - 1, 1.0f, 0);
  const int rows = WAV_INTEGRATED_WAVES_SIZE / kRowSize;
  Check(rows == kNumBanks * kNumWavesPerTable + 1, "no guard row");
  Check(!memcmp(
      wav_integrated_waves + (rows - 2) * kRowSize,
      wav_integrated_waves + (rows - 1) * kRowSize,
      kRowSize * sizeof(int16_t)), "guard row differs from the last wave");
  printf("PASS last wave of the last bank\n");
  return 0;
}

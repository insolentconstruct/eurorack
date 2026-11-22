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
// The Wave terrain blob's wavetable lookup against Plaits': equal wherever
// the path stays on the terrain; past it, clamped to the bank's edge and never
// reading outside the table (the copy sits between guards of full-scale
// samples, which would change the result).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/test/test_checker.h"
#include "beads/test/wave_terrain_lookups.h"
#include "plaits/resources.h"

const size_t kTableSize = 3 * 64 * 132;
const size_t kGuardSize = 16 * 132;

int16_t guarded[kGuardSize + kTableSize + kGuardSize];
const int16_t* waves = &guarded[kGuardSize];

void FillGuards() {
  for (size_t i = 0; i < kGuardSize; ++i) {
    const int16_t full_scale = i & 1 ? 32767 : -32768;
    guarded[i] = guarded[kGuardSize + kTableSize + i] = full_scale;
  }
  memcpy(&guarded[kGuardSize], plaits::wav_integrated_waves,
      kTableSize * sizeof(int16_t));
}

bool Inside(float x, float y) {
  const float sample = (y + 1.0f) * 0.5f * 128.0f;
  const float wt = (x + 1.0f) * 0.5f * 63.0f;
  return sample >= 0.0f && sample <= 128.0f && wt >= 0.0f &&
      wt <= 62.9999f;
}

void TestInside() {
  test_checker.Start("TestInside");
  int compared = 0;
  for (int bank = 0; bank < 3; ++bank) {
    for (int i = 0; i <= 400; ++i) {
      for (int j = 0; j <= 400; ++j) {
        const float x = -1.0f + 2.0f * float(i) / 400.0f;
        const float y = -1.0f + 2.0f * float(j) / 400.0f;
        if (!Inside(x, y)) {
          continue;
        }
        const float a = BlobTerrainLookupWT(waves, x, y, bank);
        const float b = PlaitsTerrainLookupWT(x, y, bank);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        ++compared;
      }
    }
  }
  printf("%d points inside the terrain equal Plaits'\n", compared);
}

void TestOvershoot() {
  test_checker.Start("TestOvershoot");
  // The quadrature path's norm drifts up to 2 before it is renormalised.
  const float edge = 1.5f;
  int outside = 0;
  for (int bank = 0; bank < 3; ++bank) {
    for (int i = 0; i <= 120; ++i) {
      for (int j = 0; j <= 120; ++j) {
        const float x = -edge + 2.0f * edge * float(i) / 120.0f;
        const float y = -edge + 2.0f * edge * float(j) / 120.0f;
        const float a = BlobTerrainLookupWT(waves, x, y, bank);
        CHECK(std::isfinite(a));
        if (Inside(x, y)) {
          continue;
        }
        ++outside;
        // The value at the nearest point on the terrain's edge.
        const float cx = std::min(std::max(x, -1.0f), 1.0f);
        const float cy = std::min(std::max(y, -1.0f), 1.0f);
        const float b = BlobTerrainLookupWT(waves, cx, cy, bank);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
      }
    }
  }
  printf("%d points past the terrain stay finite\n", outside);
}

int main() {
  FillGuards();
  TestInside();
  TestOvershoot();
  puts(test_checker.failures() ? "Wave terrain blob checks: FAIL"
                               : "Wave terrain blob checks: PASS");
  return test_checker.failures() ? 1 : 0;
}

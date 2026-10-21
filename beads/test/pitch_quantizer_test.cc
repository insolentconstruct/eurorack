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
// Fixed-root pitch grids and randomized pitch sampling.

#include <algorithm>
#include <cmath>

#include "beads/dsp/randomized_parameter.h"
#include "beads/test/test_checker.h"

using namespace beads;

void TestPitchGrids() {
  test_checker.Start("TestPitchGrids");
  CHECK(QuantizePitch(-0.5f, PITCH_SCALE_CHROMATIC) == -1.0f);
  CHECK(QuantizePitch(3.5f, PITCH_SCALE_FIFTHS) == 0.0f);
  CHECK(QuantizePitch(9.5f, PITCH_SCALE_FIFTHS) == 7.0f);
  CHECK(QuantizePitch(2.0f, PITCH_SCALE_MAJOR) == 2.0004f);
  CHECK(QuantizePitch(8.0f, PITCH_SCALE_MINOR) == 8.0004f);
  const float pentatonics[2][5] = {
    { 0.0f, 2.0004f, 3.9996f, 6.9996f, 9.0f },
    { 0.0f, 3.0f, 5.0004f, 6.9996f, 9.9996f }
  };
  for (int s = 0; s < 2; ++s) {
    const PitchScale scale = PitchScale(PITCH_SCALE_PENTATONIC_MAJOR + s);
    CHECK(kPitchScales[scale - 1].size == 5);
    for (int i = 0; i < 5; ++i) {
      const float lower = pentatonics[s][i];
      const float upper = i == 4 ? 12.0f : pentatonics[s][i + 1];
      const float midpoint = 0.5f * (lower + upper);
      CHECK(QuantizePitch(lower, scale) == lower);
      CHECK(QuantizePitch(midpoint - 0.0001f, scale) == lower);
      CHECK(QuantizePitch(midpoint, scale) == lower);
      CHECK(QuantizePitch(midpoint + 0.0001f, scale) == upper);
    }
  }
  CHECK(fabsf(QuantizePitch(1.6f, PITCH_SCALE_PELOG) - 1.53f) < 0.00001f);
  CHECK(fabsf(QuantizePitch(-4.0f, PITCH_SCALE_BHAIRAV) + 4.0788f)
      < 0.00001f);
  CHECK(fabsf(QuantizePitch(3.0f, PITCH_SCALE_SHRI) - 3.156f) < 0.00001f);
  for (int s = 1; s < PITCH_SCALE_LAST; ++s) {
    const PitchScale scale = PitchScale(s);
    float previous = -1000.0f;
    for (int i = -12000; i <= 12000; ++i) {
      const float pitch = float(i) * 0.01f;
      const float q = QuantizePitch(pitch, scale);
      CHECK(q >= previous);
      CHECK(fabsf(QuantizePitch(q, scale) - q) < 0.00002f);
      // Away from decision boundaries, translating an octave translates output.
      if (fabsf(pitch - q) < 0.1f) {
        CHECK(fabsf(QuantizePitch(pitch + 12.0f, scale) - q - 12.0f)
            < 0.00002f);
      }
      CHECK(QuantizePitch(pitch, PITCH_SCALE_OFF) == pitch);
      previous = q;
    }
    const PitchScaleNotes& notes = kPitchScales[s - 1];
    for (int i = 0; i < notes.size; ++i) {
      const float degree = kPitchScaleNotes[notes.offset + i];
      CHECK(QuantizePitch(degree, scale) == degree);
      if (scale >= PITCH_SCALE_FIFTHS) {
        const float upper = i + 1 == notes.size ? 12.0f
            : kPitchScaleNotes[notes.offset + i + 1];
        for (int octave = -8; octave <= 8; ++octave) {
          const float lower_pitch = float(octave * 12) + degree;
          const float upper_pitch = float(octave * 12) + upper;
          const float midpoint = 0.5f * (lower_pitch + upper_pitch);
          CHECK(QuantizePitch(midpoint, scale) == lower_pitch);
          CHECK(QuantizePitch(midpoint + 0.0001f, scale) == upper_pitch);
        }
      }
    }
  }
}

void TestRandomizedPitchGrid() {
  test_checker.Start("TestRandomizedPitchGrid");
  RandomizedParameter p;
  p.base = -3.7f;
  p.cv = 12.0f;
  for (int cv = 0; cv < 2; ++cv) {
    p.has_cv = cv;
    for (int a = -1; a <= 1; ++a) {
      p.attenuverter = float(a) * 0.7f;
      for (int i = 1; i < 100; ++i) {
        stmlib::Random::Seed(i);
        const float pitch = p.value(NULL, 0, 24.0f, false);
        const uint32_t next = stmlib::Random::GetWord();
        stmlib::Random::Seed(i);
        CHECK(p.pitch_value(PITCH_SCALE_PELOG, false) ==
            stmlib::SemitonesToRatio(QuantizePitch(pitch, PITCH_SCALE_PELOG)));
        CHECK(stmlib::Random::GetWord() == next);
        stmlib::Random::Seed(i);
        const float stock = p.pitch_value();
        stmlib::Random::Seed(i);
        CHECK(p.pitch_value(PITCH_SCALE_OFF, false) == stock);
        if (a >= 0) {
          // Random intervals only acts counter-clockwise.
          stmlib::Random::Seed(i);
          CHECK(p.pitch_value(PITCH_SCALE_OFF, true) == stock);
        }
        // Exactly: the offset snaps, then the scale; same random draws.
        for (int c = 0; c < 3; ++c) {
          p.cv = c == 0 ? 12.0f : c == 1 ? 0.0f : -5.0f;
          stmlib::Random::Seed(i);
          const float raw = p.value(NULL, 0, 24.0f, false);
          const uint32_t after = stmlib::Random::GetWord();
          const float snapped = a < 0
              ? p.base + QuantizePitch(raw - p.base, PITCH_SCALE_FIFTHS)
              : raw;
          stmlib::Random::Seed(i);
          CHECK(p.pitch_value(PITCH_SCALE_PELOG, true) ==
              stmlib::SemitonesToRatio(
                  QuantizePitch(snapped, PITCH_SCALE_PELOG)));
          CHECK(stmlib::Random::GetWord() == after);
        }
        p.cv = 12.0f;
      }
    }
  }
}

// Random intervals (#8): counter-clockwise, the random offset lands on fifths
// and octaves around the knob's pitch, and the scale still applies; clockwise
// it stays continuous.
void TestRandomIntervals() {
  test_checker.Start("TestRandomIntervals");
  RandomizedParameter p;
  p.base = 2.5f;
  p.cv = 0.0f;
  p.has_cv = false;
  int off_grid[2] = { 0, 0 };
  int moved = 0;
  for (int side = 0; side < 2; ++side) {
    p.attenuverter = side ? 0.8f : -0.8f;
    for (int i = 0; i < 2000; ++i) {
      const float offset = 12.0f * log2f(
          p.pitch_value(PITCH_SCALE_OFF, true)) - p.base;
      const float snapped = QuantizePitch(offset, PITCH_SCALE_FIFTHS);
      off_grid[side] += fabsf(offset - snapped) > 0.001f;
      moved += !side && fabsf(offset) > 0.5f;
    }
  }
  printf("Random intervals: CCW moved %d, off the grid CCW %d CW %d\n",
      moved, off_grid[0], off_grid[1]);
  CHECK(moved > 200);
  CHECK(off_grid[0] == 0 && off_grid[1] > 1900);
  // With a scale, the result is on the scale.
  p.attenuverter = -0.8f;
  for (int i = 0; i < 500; ++i) {
    const float pitch = 12.0f * log2f(p.pitch_value(PITCH_SCALE_MAJOR, true));
    CHECK(fabsf(pitch - QuantizePitch(pitch, PITCH_SCALE_MAJOR)) < 0.001f);
  }
}

int main() {
  TestPitchGrids();
  TestRandomizedPitchGrid();
  TestRandomIntervals();
  return test_checker.failures() ? 1 : 0;
}

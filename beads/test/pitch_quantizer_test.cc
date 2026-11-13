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
        CHECK(p.pitch_value(PITCH_SCALE_PELOG, RANDOM_INTERVALS_OFF) ==
            stmlib::SemitonesToRatio(QuantizePitch(pitch, PITCH_SCALE_PELOG)));
        CHECK(stmlib::Random::GetWord() == next);
        stmlib::Random::Seed(i);
        const float stock = p.pitch_value();
        stmlib::Random::Seed(i);
        CHECK(p.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_OFF) == stock);
        if (a >= 0) {
          // Random intervals only acts counter-clockwise.
          stmlib::Random::Seed(i);
          CHECK(p.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_FIFTHS) == stock);
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
          CHECK(p.pitch_value(PITCH_SCALE_PELOG, RANDOM_INTERVALS_FIFTHS) ==
              stmlib::SemitonesToRatio(
                  QuantizePitch(snapped, PITCH_SCALE_PELOG)));
          CHECK(stmlib::Random::GetWord() == after);
        }
        p.cv = 12.0f;
      }
    }
  }
}

// CCW random pitch uses fifths/octaves, then scale quantization (#8).
// CW stays continuous.
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
          p.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_FIFTHS)) - p.base;
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
    const float pitch = 12.0f * log2f(p.pitch_value(PITCH_SCALE_MAJOR, RANDOM_INTERVALS_FIFTHS));
    CHECK(fabsf(pitch - QuantizePitch(pitch, PITCH_SCALE_MAJOR)) < 0.001f);
  }
}

void TestCustomIntervalInteractions() {
  test_checker.Start("TestCustomIntervalInteractions");
  PitchScaleDefinition custom = { };
  custom.id = 15;
  custom.size = 3;
  custom.period = 13.0f;
  custom.notes[1] = 3.25f;
  custom.notes[2] = 8.125f;
  ConfigurePitchScales(&custom, 1, true);
  int changed[2] = { 0, 0 };
  for (int patched = 0; patched < 2; ++patched) {
    RandomizedParameter p;
    p.base = 2.5f;
    p.cv = 12.0f;
    p.has_cv = patched;
    for (int side = 0; side < 2; ++side) {
      p.attenuverter = side ? 0.8f : -0.8f;
      for (int i = 0; i < 256; ++i) {
        float ratio[2];
        for (int intervals = 0; intervals < 2; ++intervals) {
          stmlib::Random::Seed(0x81273849U + uint32_t(i) * 997);
          ratio[intervals] = p.pitch_value(PitchScale(15), RandomIntervals(intervals));
          const float pitch = 12.0f * log2f(ratio[intervals]);
          float phase = fmodf(pitch, custom.period);
          if (phase < 0.0f) {
            phase += custom.period;
          }
          bool on_scale = phase < 0.002f ||
              custom.period - phase < 0.002f;
          for (int note = 1; note < custom.size; ++note) {
            on_scale |= fabsf(phase - custom.notes[note]) < 0.002f;
          }
          CHECK(on_scale);
        }
        if (side) {
          CHECK(ratio[0] == ratio[1]);
        } else {
          changed[patched] += ratio[0] != ratio[1];
        }
      }
    }
    CHECK(changed[patched] > 0);
  }
  ConfigurePitchScales(NULL, 0, false);
}

void TestCustomPitchGrids() {
  test_checker.Start("TestCustomPitchGrids");
  PitchScaleDefinition bank[kMaxCustomScales] = { };
  for (size_t i = 0; i < kMaxCustomScales; ++i) {
    bank[i].id = 2 + i;
    bank[i].period = 13.0f;
    bank[i].size = 1 + i;
    for (size_t j = 0; j < bank[i].size; ++j) {
      bank[i].notes[j] = float(j) * 13.0f / float(bank[i].size);
    }
  }
  bank[0].size = 24;
  for (size_t j = 0; j < bank[0].size; ++j) {
    bank[0].notes[j] = float(j) * 13.0f / float(bank[0].size);
  }
  ConfigurePitchScales(bank, kMaxCustomScales, true);
  for (size_t s = 0; s < kMaxCustomScales; ++s) {
    const PitchScale scale = PitchScale(bank[s].id);
    float previous = -1000.0f;
    for (int i = -12000; i < 12000; ++i) {
      const float pitch = float(i) * 0.01f;
      const float q = QuantizePitch(pitch, scale);
      CHECK(q >= previous);
      CHECK(fabsf(QuantizePitch(q, scale) - q) < 0.0001f);
      CHECK(fabsf(QuantizePitch(pitch + 13.0f, scale) - q - 13.0f)
          < 0.0001f || fabsf(pitch - q) > 0.25f);
      previous = q;
    }
    for (size_t i = 0; i < bank[s].size; ++i) {
      CHECK(QuantizePitch(bank[s].notes[i], scale) == bank[s].notes[i]);
    }
  }
  // The caller may discard or edit its decoded profile after installation.
  bank[0].notes[1] = 0.01f;
  CHECK(QuantizePitch(0.5f, PITCH_SCALE_FIFTHS) > 0.5f);
  CHECK(QuantizePitch(3.5f, PITCH_SCALE_CHROMATIC) == 3.0f);
  CHECK(QuantizeFifths(3.5f) == 0.0f);
  CHECK(QuantizeFifths(9.5f) == 7.0f);
  RandomizedParameter random;
  random.base = 2.5f;
  random.cv = 0.0f;
  random.has_cv = false;
  random.attenuverter = -0.8f;
  for (int i = 0; i < 128; ++i) {
    const float offset = 12.0f * log2f(
        random.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_FIFTHS)) - random.base;
    CHECK(fabsf(offset - QuantizeFifths(offset)) < 0.001f);
  }
  CHECK(QuantizePitch(2.345f, PITCH_SCALE_OFF) == 2.345f);
  ConfigurePitchScales(NULL, 0, true);
  CHECK(QuantizePitch(2.345f, PitchScale(15)) == 2.345f);
  ConfigurePitchScales(NULL, 0, false);
  CHECK(QuantizePitch(2.0f, PITCH_SCALE_MAJOR) == 2.0004f);
}

void TestCustomPitchRatioLimits() {
  test_checker.Start("TestCustomPitchRatioLimits");
  for (int period = 0; period < 2; ++period) {
    for (int side = 0; side < 2; ++side) {
      PitchScaleDefinition bank[kMaxCustomScales] = { };
      for (size_t i = 0; i < kMaxCustomScales; ++i) {
        bank[i].id = i + 2;
        bank[i].period = period ? 128.0f : 78.0f;
        bank[i].size = period ? 1 : 2;
        bank[i].notes[1] = period ? 0.0f : side ? 24.0f : 54.0f;
        bank[i].name[0] = 'R';
      }
      ConfigurePitchScales(bank, kMaxCustomScales, true);
      RandomizedParameter p;
      // Synth::Render adds the clamped +/-60 CV to the +/-24 knob, then
      // clears has_cv. Its attenuverter can add another +/-24 of randomness.
      p.base = side ? -24.0f - 60.0f : 24.0f + 60.0f;
      p.has_cv = false;
      p.cv = 0.0f;
      p.attenuverter = 0.9999f;
      int outside = 0;
      for (size_t id = 2; id <= kMaxPitchScaleId; ++id) {
        for (int i = 0; i < 256; ++i) {
          const uint32_t seed = 0x12345678 + uint32_t(i) * 997;
          stmlib::Random::Seed(seed);
          const float raw = p.value(NULL, 0, 24.0f, false);
          const float q = QuantizePitch(raw, PitchScale(id));
          outside += q < -120.0f || q > 120.0f;
          CHECK(fabsf(raw) <= 108.0f);
          CHECK(fabsf(q) <= 144.0f);
          stmlib::Random::Seed(seed);
          const float ratio = p.pitch_value(PitchScale(id), RANDOM_INTERVALS_OFF);
          const float reference = powf(2.0f, q / 12.0f);
          CHECK(ratio > 0.0f && ratio <= 4096.0f);
          CHECK(fabsf(ratio / reference - 1.0f) < 0.001f);
          CHECK(ratio == stmlib::SemitonesToRatioSafe(q));
        }
      }
      CHECK(outside > 0);
      const float off = stmlib::SemitonesToRatio(p.base);
      p.has_cv = true;
      p.attenuverter = 0.0f;
      CHECK(p.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_OFF) == off);
    }
  }
  ConfigurePitchScales(NULL, 0, false);
  // Keep the factory path bit-identical over every reachable synth input.
  RandomizedParameter p;
  p.cv = 0.0f;
  p.has_cv = true;
  p.attenuverter = 0.0f;
  for (int scale = PITCH_SCALE_OFF; scale < PITCH_SCALE_LAST; ++scale) {
    for (int i = -10800; i <= 10800; i += 11) {
      p.base = float(i) * 0.01f;
      const float expected = stmlib::SemitonesToRatio(
          QuantizePitch(p.base, PitchScale(scale)));
      CHECK(p.pitch_value(PitchScale(scale), RANDOM_INTERVALS_OFF) == expected);
    }
  }
}

// Recent notes: a note counts once the CV has held for 10 ms; grains pick
// among the last 1 to 8 of them, more counter-clockwise (#220).
void TestRecentNotes() {
  test_checker.Start("TestRecentNotes");
  uint32_t now = 0;
  RememberNote(0.0f, false, now);
  CHECK(RecentNote(1.0f, 4.5f) == 4.5f);
  // A slewed or noisy CV is not a note until it settles.
  for (int i = 0; i < 40; ++i) {
    RememberNote(0.2f * float(i), true, ++now);
  }
  CHECK(RecentNote(1.0f, 7.0f) == 7.0f);
  for (int i = 0; i < 9; ++i) {
    RememberNote(i & 1 ? 2.03f : 1.97f, true, ++now);
  }
  CHECK(RecentNote(1.0f, 7.0f) == 7.0f);
  RememberNote(2.0f, true, ++now);
  RememberNote(2.0f, true, ++now);
  CHECK(RecentNote(1.0f, 7.0f) == 2.0f);
  // A melody: C E G B D, then a quarter-tone wobble that is not a new note.
  const float melody[] = { 0.0f, 4.0f, 7.0f, 11.0f, 14.0f, 14.3f };
  for (int n = 0; n < 6; ++n) {
    for (int i = 0; i < 20; ++i) {
      RememberNote(melody[n], true, ++now);
    }
  }
  // Fully clockwise of the counter-clockwise half: only the newest note.
  for (int i = 0; i < 200; ++i) {
    CHECK(RecentNote(0.01f, -9.0f) == 14.0f);
  }
  // Fully counter-clockwise: every remembered note, and only those.
  const float stored[6] = { 2.0f, 0.0f, 4.0f, 7.0f, 11.0f, 14.0f };
  int seen[6] = { 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < 3000; ++i) {
    const float note = RecentNote(1.0f, -9.0f);
    int found = -1;
    for (int n = 0; n < 6; ++n) {
      found = note == stored[n] ? n : found;
    }
    CHECK(found >= 0);
    seen[found >= 0 ? found : 0] += found >= 0;
  }
  for (int n = 0; n < 6; ++n) {
    CHECK(seen[n] > 400 && seen[n] < 600);
  }
  // Half way: the newest 1 + round(0.5 x 7) = 5 notes.
  for (int i = 0; i < 2000; ++i) {
    CHECK(RecentNote(0.5f, -9.0f) != 2.0f);
  }
  // Only the last eight are kept.
  for (int n = 0; n < 12; ++n) {
    for (int i = 0; i < 11; ++i) {
      RememberNote(float(n) * -1.0f - 20.0f, true, ++now);
    }
  }
  for (int i = 0; i < 2000; ++i) {
    const float note = RecentNote(1.0f, 0.0f);
    CHECK(note <= -24.0f && note >= -31.0f);
  }
  // Unplugging forgets; the clock may wrap.
  RememberNote(3.0f, false, ++now);
  CHECK(RecentNote(1.0f, 3.0f) == 3.0f);
  now = 0xfffffffa;
  for (int i = 0; i < 12; ++i) {
    RememberNote(5.0f, true, ++now);
  }
  CHECK(RecentNote(1.0f, 0.0f) == 5.0f);

  // Grains: CW and Free stay stock; CCW picks base + a recent note.
  RandomizedParameter p;
  p.base = -3.0f;
  p.cv = 9.0f;
  p.has_cv = true;
  for (int a = -1; a <= 1; ++a) {
    p.attenuverter = 0.6f * float(a);
    for (int i = 1; i < 200; ++i) {
      stmlib::Random::Seed(i);
      const float stock = p.pitch_value(PITCH_SCALE_OFF, RANDOM_INTERVALS_OFF);
      stmlib::Random::Seed(i);
      const float recent = p.pitch_value(PITCH_SCALE_OFF,
          RANDOM_INTERVALS_RECENT_NOTES);
      if (a >= 0) {
        CHECK(recent == stock);
      } else {
        CHECK(recent == stmlib::SemitonesToRatio(-3.0f + 5.0f));
      }
    }
  }
  // Unpatched, Recent notes is Free.
  p.has_cv = false;
  p.attenuverter = -0.6f;
  for (int i = 1; i < 200; ++i) {
    stmlib::Random::Seed(i);
    const float stock = p.pitch_value(PITCH_SCALE_MAJOR, RANDOM_INTERVALS_OFF);
    stmlib::Random::Seed(i);
    CHECK(p.pitch_value(PITCH_SCALE_MAJOR, RANDOM_INTERVALS_RECENT_NOTES)
        == stock);
  }
  // A scale still applies afterwards.
  p.has_cv = true;
  RememberNote(0.0f, false, ++now);
  for (int i = 0; i < 12; ++i) {
    RememberNote(1.0f, true, ++now);
  }
  CHECK(p.pitch_value(PITCH_SCALE_MAJOR, RANDOM_INTERVALS_RECENT_NOTES) ==
      stmlib::SemitonesToRatio(QuantizePitch(-2.0f, PITCH_SCALE_MAJOR)));
  RememberNote(0.0f, false, ++now);
}

int main() {
  TestPitchGrids();
  TestCustomIntervalInteractions();
  TestCustomPitchGrids();
  TestCustomPitchRatioLimits();
  TestRandomizedPitchGrid();
  TestRandomIntervals();
  TestRecentNotes();
  return test_checker.failures() ? 1 : 0;
}

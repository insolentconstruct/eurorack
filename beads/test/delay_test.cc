// Copyright 2026 Insolent Construct.
//
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
// Frozen delay slice tests.

#include "beads/test/test_fpu.h"

#include <stdint.h>
#include <cstdio>
#include <cmath>

#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/delay_controller.h"
#undef private
#include "beads/dsp/delay_line.h"
// Include the table guards as well as the reachable ratios.
#include "beads/dsp/delay_controller.cc"

using namespace beads;
using namespace stmlib;

const int kMaxDelay = 192000;
const int kStartOffset = 64 + 4;  // kMinDelay in delay_controller.cc.
typedef DelayLine<RESOLUTION_16_BIT, kMaxDelay, 2> Line;
static uint8_t line_data[3 * 262144 + 2048];

// Render delay and check every tap. SEED pulses every `period` samples from
// `offset` (period 0: no external clock, free-running). Frozen from
// `freeze_at`. Always asserts valid slices and finite output. Returns the
// sounding-tap count for the longer-period audible-output checks only.
int CheckValidSlices(int period, int offset, int freeze_at, float density, float fm) {
  DelayController c;
  c.Init(48000, kMaxDelay);
  Line line;
  line.Init(line_data, sizeof(line_data));
  RandomizedParameter position = { 1.0f, 0.0f, 0.0f, false };
  RandomizedParameter shape = { 0.5f, 0.0f, 0.0f, false };
  DelayController::Output o;
  int frozen_taps = 0;
  for (int n = 0; n < 200000; ++n) {
    bool frozen = n >= freeze_at;
    GateFlags g = GATE_FLAG_LOW;
    if (period) {
      const int k = n < offset ? period / 2 + 1 : (n - offset) % period;
      g = k == 0 ? GATE_FLAG_HIGH | GATE_FLAG_RISING
          : k < period / 2 ? GATE_FLAG_HIGH
          : k == period / 2 ? GATE_FLAG_FALLING : GATE_FLAG_LOW;
    }
    c.Render(
        period ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_LOCKED,
        period ? &g : NULL,
        &frozen, density, -0.3f, fm, position, shape, &o, 1);
    for (int i = 0; i < DelayController::NUM_HEADS; ++i) {
      CHECK(std::isfinite(c.play_head_[i].phase_increment_));
      CHECK(c.play_head_[i].phase_increment_ >= 0.0f);
      CHECK(c.play_head_[i].remaining_ >= 0);
      CHECK(c.play_head_[i].phase_increment_ <= 1.0f);
    }
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
      CHECK(std::isfinite(o.tap[i].gain));
      const FloatFrame sample = line.Read<INTERPOLATION_HERMITE>(o.tap[i].delay);
      CHECK(std::isfinite(sample.l) && std::isfinite(sample.r));
      CHECK(o.tap[i].delay >= 0);
      CHECK(o.tap[i].delay <= (kMaxDelay + kStartOffset) * 1024);
      if (o.tap[i].gain > 0.00001f) {
        line.Read<INTERPOLATION_HERMITE>(o.tap[i].delay);
        frozen_taps += frozen;
      }
    }
  }
  return frozen_taps;
}

void TestFrozenSliceBoundarySelection() {
  test_checker.Start("TestFrozenSliceBoundarySelection");
  for (int clocked = 0; clocked < 2; ++clocked) {
    DelayController c;
    c.Init(48000, kMaxDelay);
    RandomizedParameter position = { 0.1f, 0.0f, 0.0f, false };
    RandomizedParameter shape = { 0.5f, 0.0f, 0.0f, false };
    bool frozen = true;
    DelayController::Output o;
    bool pending = false;
    int unchanged = 0, changed = 0;
    for (int n = 0; n < 80000; ++n) {
      if (n == 40000) {
        position.base = 0.9f;
        pending = true;
      }
      PlayHead* head[DelayController::NUM_TAPS];
      int start[DelayController::NUM_TAPS];
      int remaining[DelayController::NUM_TAPS];
      for (int i = 0; i < DelayController::NUM_HEADS; ++i) {
        head[2 * i] = &c.play_head_[i];
        head[2 * i + 1] = &c.slave_play_head_[i];
      }
      for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
        start[i] = head[i]->start_;
        remaining[i] = head[i]->remaining_;
      }
      GateFlags g = n % 4096 == 0 ? GATE_FLAG_RISING : GATE_FLAG_LOW;
      c.Render(clocked ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_LOCKED,
          clocked ? &g : NULL, &frozen, 0.25f, 0.0f, 0.0f,
          position, shape, &o, 1);
      if (!pending || (clocked && c.clock_.retrig())) {
        continue;
      }
      for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
        if (remaining[i] > 0) {
          CHECK(head[i]->start_ == start[i]);
          ++unchanged;
        } else if (head[i]->start_ != start[i]) {
          CHECK(head[i]->start_ > start[i]);
          ++changed;
        }
      }
    }
    CHECK(unchanged > 0 && changed > 0);
  }
}

void TestClockDividerTable() {
  test_checker.Start("TestClockDividerTable");
  const float expected[] = {
    1.0f / 16, 1.0f / 16, 1.0f / 8, 1.0f / 8, 1.0f / 4, 1.0f / 4,
    1.0f / 2, 1.0f / 2, 1, 1, 1, 1, 1.0f / 2, 1.0f / 3, 1.0f / 4,
    1.0f / 6, 1.0f / 8, 1.0f / 10, 1.0f / 12,
    1.0f / 16, 1.0f / 16, 1.0f / 16
  };
  for (int i = 0; i < 22; ++i) {
    CHECK(kRatio[i] == expected[i]);
  }
  Line line;
  line.Init(line_data, sizeof(line_data));
  for (int index = 0; index < 20; ++index) {
    DelayController c;
    c.Init(48000, kMaxDelay);
    RandomizedParameter position = { 0.8f, 0.0f, 0.0f, false };
    RandomizedParameter shape = { 0.5f, 0.0f, 0.0f, false };
    bool frozen = true;
    DelayController::Output o;
    const float density = (float(index) + 0.5f) / 20.0f;
    int checked = 0;
    for (int n = 0; n < 24000; ++n) {
      GateFlags g = n % 128 == 0 ? GATE_FLAG_RISING : GATE_FLAG_LOW;
      c.Render(SEED_BUTTON_STATE_OFF, &g, &frozen, density, 0, 0,
          position, shape, &o, 1);
      for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
        CHECK(o.tap[i].delay >= 0);
        CHECK(o.tap[i].delay <= (kMaxDelay + kStartOffset) * 1024);
        line.Read<INTERPOLATION_HERMITE>(o.tap[i].delay);
      }
      if (n < 12000) {
        continue;
      }
      CHECK(int(c.density_ * 20) == index);
      for (int i = 0; i < DelayController::NUM_HEADS; ++i) {
        const PlayHead& h = c.play_head_[i];
        const int ratio_index = index + ((i % 2) != (index & 1));
        const int duration = std::max(1, int(c.clock_.interval(i / 2) *
            expected[ratio_index]));
        CHECK(fabsf(h.phase_increment_ * duration - 1.0f) < 0.00001f);
        CHECK(duration >= 1);
        if (h.remaining_ == duration - 1) {
          ++checked;
        }
      }
    }
    CHECK(checked > 0);
  }
}

void TestDelayIntervals() {
  test_checker.Start("TestDelayIntervals");
  for (size_t i = 0; i < sizeof(line_data); ++i) {
    line_data[i] = uint8_t(i * 17 + (i >> 8));
  }
  // Two SEED edges under 16 samples apart, or the first edge after Init, made
  // a zero-length slice at the 1/16 ratio: an integer division by zero.
  // Such slices now last one sample. This prevents invalid slices and
  // non-finite output, but does not guarantee audible output.
  const int short_periods[] = { 2, 4, 8, 15 };
  for (size_t i = 0;
       i < sizeof(short_periods) / sizeof(short_periods[0]); ++i) {
    CheckValidSlices(short_periods[i], 0, 0, 0.0f, 0.0f);
    CheckValidSlices(short_periods[i], 0, 50000, 0.0f, 0.0f);
    CheckValidSlices(short_periods[i], 1000, 50000, 0.0f, 0.0f);
  }
  const int periods[] = { 64, 12000 };
  for (size_t i = 0; i < sizeof(periods) / sizeof(periods[0]); ++i) {
    CheckValidSlices(periods[i], 0, 0, 0.0f, 0.0f);
    CHECK(CheckValidSlices(periods[i], 1000, 50000, 0.0f, 0.0f) > 0);
  }
  // Negative exponential FM made a slice longer than the buffer.
  const float fms[] = { -0.3f, -0.7f, 0.0f, 0.5f };
  for (size_t i = 0; i < sizeof(fms) / sizeof(fms[0]); ++i) {
    CHECK(CheckValidSlices(0, 0, 50000, 0.76f, fms[i]) > 0);
    CheckValidSlices(0, 0, 0, 0.76f, fms[i]);
  }
}

void TestClockIdleCounter() {
  test_checker.Start("TestClockIdleCounter");
  DelayClock clock;
  clock.Init(48000.0f);
  // Hours without a tick: the counter stops at the limit instead of wrapping.
  clock.next_interval_ = 2147483647 - 4;
  for (int i = 0; i < 16; ++i) {
    clock.Process(false, false);
  }
  CHECK(clock.next_interval_ == clock.max_interval_);
  // The next tick is ignored, as after any gap longer than 4 s.
  const float interval_0 = clock.interval(0);
  const float interval_1 = clock.interval(1);
  clock.Process(true, false);
  CHECK(clock.next_interval_ == 0);
  CHECK(clock.interval(0) == interval_0 && clock.interval(1) == interval_1);
}

void RenderFree(DelayController* controller, float density, float cv,
    float time, DelayController::Output* out) {
  RandomizedParameter position = { time, 0.0f, 0.0f, false };
  RandomizedParameter shape = { 0.0f, 0.0f, 0.0f, false };
  bool freeze = false;
  controller->Render(SEED_BUTTON_STATE_LOCKED, NULL, &freeze,
      density, cv, 0.0f, position, shape, out, 1);
}

void TestFreeDelayDensityAndCv() {
  test_checker.Start("TestFreeDelayDensityAndCv");
  const float density[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
  // Ascending controls settle 0.0025 below each nonzero input.
  const float expected[] = { 375.0f, 8224.695f, 186103.5f,
      8754.125f, 386.881f };
  for (int cv = 0; cv < 2; ++cv) {
    for (int i = 0; i < 5; ++i) {
      DelayController controller;
      controller.Init(48000, kMaxDelay);
      DelayController::Output out;
      for (int n = 0; n < 16000; ++n) {
        RenderFree(&controller, cv ? 0.0f : density[i],
            cv ? density[i] : 0.0f, 0.0f, &out);
      }
      CHECK(fabsf(controller.clock_.interval(0) - expected[i]) <
          expected[i] * 0.001f);
    }
  }
}

void TestSecondaryTapModulation() {
  test_checker.Start("TestSecondaryTapModulation");
  DelayController controller;
  controller.Init(48000, kMaxDelay);
  DelayController::Output out;
  float minimum = 1.0f, maximum = 0.0f, previous = 0.0f;
  for (int n = 0; n < 300000; ++n) {
    RenderFree(&controller, 0.75f, 0.0f, 0.0f, &out);
    if (n < 16000) {
      continue;
    }
    const float gain = out.tap[6].gain;
    if (!CHECK(gain >= 0.199f && gain <= 0.801f)) {
      return;
    }
    if (n > 16000 && !CHECK(fabsf(gain - previous) < 0.0001f)) {
      return;
    }
    previous = gain;
    minimum = std::min(minimum, gain);
    maximum = std::max(maximum, gain);
    if (!CHECK(fabsf(float(out.tap[6].delay) / out.tap[4].delay - 0.77f)
        < 0.00001f)) {
      return;
    }
  }
  CHECK(maximum - minimum > 0.1f);
  controller.Init(48000, kMaxDelay);
  for (int n = 0; n < 16000; ++n) {
    RenderFree(&controller, 0.25f, 0.0f, 0.0f, &out);
  }
  CHECK(out.tap[6].gain == 0.0f && out.tap[4].gain == 1.0f);
}

void TestSeedClockIntervalsAndCrossfade() {
  test_checker.Start("TestSeedClockIntervalsAndCrossfade");
  for (int tap = 0; tap < 2; ++tap) {
    DelayClock tapped;
    tapped.Init(48000);
    tapped.Process(!tap, tap);
    for (int n = 0; n < 5999; ++n) {
      tapped.Process(false, false);
    }
    tapped.Process(!tap, tap);
    CHECK(tapped.interval(tapped.new_interval_index()) == 6000.0f);
    DelayClock clock;
    clock.Init(48000);
    for (int n = 0; n < 6000; ++n) {
      clock.Process(false, false);
    }
    clock.Process(!tap, tap);
    const int current = clock.new_interval_index();
    CHECK(clock.interval(current) == 6001.0f);
    CHECK(clock.interval(1 - current) == 12000.0f);
    for (int n = 0; n < 100; ++n) {
      clock.Process(false, false);
    }
    CHECK(clock.gain(0) > 0.0f && clock.gain(1) > 0.0f);
    CHECK(fabsf(clock.gain(0) + clock.gain(1) - 1.0f) < 0.00001f);
    for (int n = 100; n < 192000 - 1; ++n) {
      clock.Process(false, false);
    }
    clock.Process(!tap, tap);
    CHECK(clock.interval(current) == 6001.0f);
    CHECK(clock.interval(1 - current) == 12000.0f);
  }
  DelayController controller;
  controller.Init(48000, kMaxDelay);
  RandomizedParameter position = { 0.0f, 0.0f, 0.0f, false };
  RandomizedParameter shape = { 0.0f, 0.0f, 0.0f, false };
  bool freeze = false;
  DelayController::Output out;
  for (int n = 0; n <= 6101; ++n) {
    GateFlags gate = n == 6000 ? GATE_FLAG_RISING : GATE_FLAG_LOW;
    controller.Render(SEED_BUTTON_STATE_OFF, &gate, &freeze,
        0.425f, 0.0f, 0.0f, position, shape, &out, 1);
  }
  CHECK(out.tap[0].gain > 0.0f && out.tap[4].gain > 0.0f);
  CHECK(out.tap[0].delay == 6001 * 1024);
  CHECK(out.tap[4].delay == 12000 * 1024);
}

void TestTimeMultipliersAndCrossfade() {
  test_checker.Start("TestTimeMultipliersAndCrossfade");
  for (int multiple = 1; multiple <= 16; ++multiple) {
    DelayController controller;
    controller.Init(48000, kMaxDelay);
    DelayController::Output out;
    const float time = float(multiple - 1) / (15.0f * 1.005f);
    for (int n = 0; n < 16000; ++n) {
      RenderFree(&controller, 0.0f, 0.0f, time, &out);
    }
    int sounding = 0;
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
      if (out.tap[i].gain > 0.001f) {
        CHECK(abs(out.tap[i].delay - multiple * 375 * 1024) < 2048);
        ++sounding;
      }
    }
    CHECK(sounding == 1);
  }
  DelayController controller;
  controller.Init(48000, kMaxDelay);
  DelayController::Output out;
  for (int n = 0; n < 16000; ++n) {
    RenderFree(&controller, 0.0f, 0.0f, 0.0f, &out);
  }
  bool overlap = false;
  for (int n = 0; n < 1000; ++n) {
    RenderFree(&controller, 0.0f, 0.0f, 1.0f / 15.075f, &out);
    if (out.tap[4].gain > 0.01f && out.tap[5].gain > 0.01f) {
      CHECK(abs(out.tap[4].delay * 2 - out.tap[5].delay) <= 1);
      CHECK(fabsf(out.tap[4].gain + out.tap[5].gain - 1.0f) < 0.00001f);
      overlap = true;
    }
  }
  CHECK(overlap);
}

void TestDelayShapeEnvelope() {
  test_checker.Start("TestDelayShapeEnvelope");
  TapEnvelope envelope;
  envelope.Init();
  envelope.set_phase_increment(1.0f / 128.0f);
  for (int n = 0; n < 256; ++n) {
    CHECK(envelope.Render(0.0f) == 1.0f);
  }
  CHECK(envelope.phase() == 0.0f);
  for (int n = 0; n < 256; ++n) {
    const float phase = float(n % 128) / 128.0f;
    const float expected = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
    CHECK(fabsf(envelope.Render(0.6f) - expected) < 0.00001f);
  }
}

void TestDelayRandomModulation() {
  test_checker.Start("TestDelayRandomModulation");
  float sequence[2000];
  for (int run = 0; run < 2; ++run) {
    Random::Seed(12345);
    DelayController controller;
    controller.Init(48000, kMaxDelay);
    RandomizedParameter position = { 0.5f, 0.4f, 0.0f, false };
    RandomizedParameter shape = { 0.0f, 0.0f, 0.0f, false };
    bool freeze = false;
    DelayController::Output out;
    float previous = 0.0f;
    for (int n = 0; n < 516000; ++n) {
      controller.Render(SEED_BUTTON_STATE_LOCKED, NULL, &freeze,
          0.0f, 0.0f, 0.0f, position, shape, &out, 1);
      if (n < 16000 || n % 250) {
        continue;
      }
      const int index = (n - 16000) / 250;
      const float value = controller.multiplier_lp_;
      CHECK(value >= -1.0f && value <= 1.0f);
      // SmoothStep slope <= 1.5, with samples 250 frames apart.
      if (index && !CHECK(fabsf(value - previous) < 0.0026f)) {
        return;
      }
      previous = value;
      if (run) {
        CHECK(value == sequence[index]);
      } else {
        sequence[index] = value;
      }
    }
  }
  CHECK(fabsf(sequence[1500] - sequence[0]) > 0.01f);
}

// The grains read the stored integers and scale once; the taps read floats.
// Both must give the same samples on every line format, across the bank edges
// (1024-byte banks: every format, mono and stereo, spans several) and the wrap
// of the write pointer. Exact bit identity on the Cortex-M7 is checked by the
// Seed3 output hashes; here a compiler that fuses is allowed to differ.
template<Resolution resolution, int num_channels>
void CheckHermiteRaw(uint32_t* seed) {
  typedef DelayLine<resolution, 1500, num_channels, 1024, 6> SmallLine;
  static uint8_t data[6 * 1024 + 2048];
  SmallLine line;
  line.Init(data, sizeof(data));
  int mismatches = 0;
  for (int n = 0; n < 6000; ++n) {
    FloatFrame in;
    *seed = *seed * 1664525u + 1013904223u;
    in.l = float(int32_t(*seed >> 8) - (1 << 23)) / float(1 << 23);
    *seed = *seed * 1664525u + 1013904223u;
    in.r = float(int32_t(*seed >> 8) - (1 << 23)) / float(1 << 23);
    line.Write(in);
    if (n < 800) {
      continue;
    }
    for (int k = 0; k < 8; ++k) {
      *seed = *seed * 1664525u + 1013904223u;
      const int integral = int(*seed >> 16) % 1400;
      const float fractional = float((*seed >> 6) & 1023) / 1024.0f;
      const FloatFrame a = line.template Read<INTERPOLATION_HERMITE>(
          integral, fractional);
      const FloatFrame b = line.template Read<INTERPOLATION_HERMITE_RAW>(
          integral, fractional);
#ifdef __FP_FAST_FMAF
      // A compiler that fuses may round the two spellings differently.
      mismatches += !(fabsf(a.l - b.l) < 1.0e-6f && fabsf(a.r - b.r) < 1.0e-6f);
#else
      mismatches += !(a.l == b.l && a.r == b.r);
#endif
    }
  }
  CHECK(mismatches == 0);
}

void TestHermiteRawMatchesScaled() {
  test_checker.Start("TestHermiteRawMatchesScaled");
  uint32_t seed = 1;
  CheckHermiteRaw<RESOLUTION_16_BIT, 2>(&seed);
  CheckHermiteRaw<RESOLUTION_16_BIT, 1>(&seed);
  CheckHermiteRaw<RESOLUTION_12_BIT, 2>(&seed);
  CheckHermiteRaw<RESOLUTION_12_BIT, 1>(&seed);
  CheckHermiteRaw<RESOLUTION_8_BIT, 2>(&seed);
  CheckHermiteRaw<RESOLUTION_8_BIT, 1>(&seed);
}

int main() {
  InitTestFpu();
  TestFrozenSliceBoundarySelection();
  TestClockDividerTable();
  TestDelayIntervals();
  TestClockIdleCounter();
  TestFreeDelayDensityAndCv();
  TestSecondaryTapModulation();
  TestSeedClockIntervalsAndCrossfade();
  TestTimeMultipliersAndCrossfade();
  TestDelayShapeEnvelope();
  TestDelayRandomModulation();
  TestHermiteRawMatchesScaled();
  return test_checker.failures() ? 1 : 0;
}

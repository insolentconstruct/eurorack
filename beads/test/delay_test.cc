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
// Frozen delay slice tests.

#include "beads/test/test_checker.h"
#include <cstdint>
#include <cstdio>
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
// `freeze_at`. Returns the number of sounding taps while frozen.
int Check(int period, int offset, int freeze_at, float density, float fm) {
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
    for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
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
      if (n == 40000) { position.base = 0.9f; pending = true; }
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
      if (!pending || (clocked && c.clock_.retrig())) continue;
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
    1.0f/16, 1.0f/16, 1.0f/8, 1.0f/8, 1.0f/4, 1.0f/4,
    1.0f/2, 1.0f/2, 1, 1, 1, 1, 1.0f/2, 1.0f/3, 1.0f/4,
    1.0f/6, 1.0f/8, 1.0f/10, 1.0f/12, 1.0f/16, 1.0f/16, 1.0f/16
  };
  for (int i = 0; i < 22; ++i) CHECK(kRatio[i] == expected[i]);
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
      if (n < 12000) continue;
      CHECK(int(c.density_ * 20) == index);
      for (int i = 0; i < DelayController::NUM_HEADS; ++i) {
        const PlayHead& h = c.play_head_[i];
        const int ratio_index = index + ((i % 2) != (index & 1));
        const int duration = std::max(1, int(c.clock_.interval(i / 2) *
            expected[ratio_index]));
        CHECK(fabsf(h.phase_increment_ * duration - 1.0f) < 0.00001f);
        CHECK(duration >= 1);
        if (h.remaining_ == duration - 1) ++checked;
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
  // Such slices now last one sample, which plays silently, as before.
  const int short_periods[] = { 2, 4, 8, 15 };
  for (size_t i = 0; i < sizeof(short_periods) / sizeof(short_periods[0]); ++i) {
    Check(short_periods[i], 0, 0, 0.0f, 0.0f);
    Check(short_periods[i], 0, 50000, 0.0f, 0.0f);
    Check(short_periods[i], 1000, 50000, 0.0f, 0.0f);
  }
  const int periods[] = { 64, 12000 };
  for (size_t i = 0; i < sizeof(periods) / sizeof(periods[0]); ++i) {
    Check(periods[i], 0, 0, 0.0f, 0.0f);
    CHECK(Check(periods[i], 1000, 50000, 0.0f, 0.0f) > 0);
  }
  printf("PASS clocked delay: short and start-up clock intervals\n");
  // Negative exponential FM made a slice longer than the buffer.
  const float fms[] = { -0.3f, -0.7f, 0.0f, 0.5f };
  for (size_t i = 0; i < sizeof(fms) / sizeof(fms[0]); ++i) {
    CHECK(Check(0, 0, 50000, 0.76f, fms[i]) > 0);
    Check(0, 0, 0, 0.76f, fms[i]);
  }
  printf("PASS free-running delay: FM from -0.7 to +0.5\n");
}

int main() {
  TestFrozenSliceBoundarySelection();
  TestClockDividerTable();
  TestDelayIntervals();
  return test_checker.failures() ? 1 : 0;
}

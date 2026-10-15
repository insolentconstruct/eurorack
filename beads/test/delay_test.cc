// Host tests for frozen delay slices (#33): no division by zero, every tap's
// delay within the buffer, and every sounding tap read from a real delay line.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "beads/dsp/delay_controller.h"
#include "beads/dsp/delay_line.h"

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
      assert(o.tap[i].delay >= 0);
      assert(o.tap[i].delay <= (kMaxDelay + kStartOffset) * 1024);
      if (o.tap[i].gain > 0.00001f) {
        line.Read<INTERPOLATION_HERMITE>(o.tap[i].delay);
        frozen_taps += frozen;
      }
    }
  }
  return frozen_taps;
}

int main(int argc, char**) {
  assert(argc == 2);
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
    assert(Check(periods[i], 1000, 50000, 0.0f, 0.0f) > 0);
  }
  printf("PASS clocked delay: short and start-up clock intervals\n");
  // Negative exponential FM made a slice longer than the buffer.
  const float fms[] = { -0.3f, -0.7f, 0.0f, 0.5f };
  for (size_t i = 0; i < sizeof(fms) / sizeof(fms[0]); ++i) {
    assert(Check(0, 0, 50000, 0.76f, fms[i]) > 0);
    Check(0, 0, 0, 0.76f, fms[i]);
  }
  printf("PASS free-running delay: FM from -0.7 to +0.5\n");
}

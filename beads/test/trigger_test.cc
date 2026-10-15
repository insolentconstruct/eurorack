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
// Grain trigger modes and timing.

#include <cmath>

#include "beads/dsp/trigger_generator.h"
#include "beads/test/test_checker.h"
#include "stmlib/utils/random.h"

using namespace beads;
using namespace stmlib;

const int kSampleRate = 48000;

int Count(SeedButtonState state, float density, float linear_fm,
    float exponential_fm, int samples, int* shortest, int* longest,
    bool external = false) {
  TriggerGenerator generator;
  generator.Init(kSampleRate);
  Random::Seed(1234567);
  int count = 0;
  int previous = -1;
  *shortest = samples;
  *longest = 0;
  for (int i = 0; i < samples; ++i) {
    TriggerGenerator::Output o;
    GateFlags flags = GATE_FLAG_HIGH;
    generator.Render(external ? SEED_BUTTON_STATE_OFF : state,
        external ? &flags : NULL, density, linear_fm, exponential_fm, &o, 1);
    CHECK(o.fractional_sample >= 0.0f && o.fractional_sample <= 1.0f);
    if (o.trigger) {
      if (previous >= 0) {
        const int interval = i - previous;
        if (interval < *shortest) *shortest = interval;
        if (interval > *longest) *longest = interval;
      }
      previous = i;
      ++count;
    }
  }
  return count;
}

void TestDensityModulation() {
  test_checker.Start("TestDensityModulation");
  int shortest, longest;
  const int slow = Count(SEED_BUTTON_STATE_LOCKED, 0.2f, 0, 0,
      kSampleRate * 4, &shortest, &longest);
  const int fast = Count(SEED_BUTTON_STATE_LOCKED, 0.2f, -0.1f, 0,
      kSampleRate * 4, &shortest, &longest);
  CHECK(slow > 0 && fast > slow * 2);
  const int held = Count(SEED_BUTTON_STATE_ON, 0.1f, 0, 0,
      kSampleRate * 4, &shortest, &longest);
  CHECK(held == fast);
  CHECK(Count(SEED_BUTTON_STATE_OFF, 0.2f, 0, 0,
      kSampleRate * 4, &shortest, &longest, true) == slow);
  CHECK(Count(SEED_BUTTON_STATE_OFF, 0.1f, 0, 0,
      kSampleRate * 4, &shortest, &longest, true) == fast);
}

void TestLatchedNoonSilence() {
  test_checker.Start("TestLatchedNoonSilence");
  const float densities[] = { 0.48f, 0.5f, 0.52f };
  for (int j = 0; j < 3; ++j) {
    TriggerGenerator generator;
    generator.Init(kSampleRate);
    TriggerGenerator::Output out[48];
    generator.Render(SEED_BUTTON_STATE_LOCKED, NULL, 0, 0, 0, out, 48);
    for (int block = 0; block < 1000; ++block) {
      generator.Render(SEED_BUTTON_STATE_LOCKED, NULL, densities[j],
          0, 0, out, 48);
      for (int i = 0; i < 48; ++i) {
        CHECK(!out[i].trigger && !out[i].gate);
        CHECK(out[i].fractional_sample == 0.0f);
      }
    }
  }
}

void TestRegularDensityRate() {
  test_checker.Start("TestRegularDensityRate");
  int shortest, longest;
  const int count = Count(SEED_BUTTON_STATE_LOCKED, 0, 0, 0,
      kSampleRate * 10, &shortest, &longest);
  CHECK(std::abs(count - 1308) <= 2);
  CHECK(shortest == 366 && longest == 367);
  const int slower = Count(SEED_BUTTON_STATE_LOCKED, 0.1f, 0, 0,
      kSampleRate * 10, &shortest, &longest);
  CHECK(slower < count / 2);
  CHECK(longest - shortest <= 1);
}

void TestRandomDensityRate() {
  test_checker.Start("TestRandomDensityRate");
  int shortest, longest;
  const int slow = Count(SEED_BUTTON_STATE_LOCKED, 0.8f, 0, 0,
      kSampleRate * 20, &shortest, &longest);
  CHECK(longest > shortest * 2);
  const int fast = Count(SEED_BUTTON_STATE_LOCKED, 1, 0, 0,
      kSampleRate * 20, &shortest, &longest);
  CHECK(longest > shortest * 2);
  CHECK(fast > slow * 4);
  // Mean interval is 0.883 / 65.41 Hz: about 1481 grains in 20 s, with a
  // standard deviation near 20.
  CHECK(fast > 1360 && fast < 1600);
}

void TestClockedNoonSilence() {
  test_checker.Start("TestClockedNoonSilence");
  TriggerGenerator generator;
  generator.Init(kSampleRate);
  for (int i = 0; i < kSampleRate; ++i) {
    GateFlags flags = i % 100 == 0
        ? GATE_FLAG_HIGH | GATE_FLAG_RISING : GATE_FLAG_LOW;
    TriggerGenerator::Output o;
    generator.Render(SEED_BUTTON_STATE_LOCKED, &flags, 0.5f, 0, 0, &o, 1);
    CHECK(!o.trigger && !o.gate);
  }
}

void TestClockProbability() {
  test_checker.Start("TestClockProbability");
  const float densities[] = { 0.52f, 0.65f, 0.8f, 0.95f, 1.0f };
  for (int j = 0; j < 5; ++j) {
    TriggerGenerator generator;
    generator.Init(kSampleRate);
    Random::Seed(1234567);
    int accepted = 0;
    const int trials = 4000;
    const int period = 1200;
    for (int i = 0; i < trials * period; ++i) {
      GateFlags flags = i % period ? GATE_FLAG_LOW
          : GATE_FLAG_HIGH | GATE_FLAG_RISING;
      TriggerGenerator::Output o;
      generator.Render(SEED_BUTTON_STATE_LOCKED, &flags, densities[j],
          0, 0, &o, 1);
      if (o.trigger) ++accepted;
      CHECK(!o.trigger || i % period == 0);
    }
    const float root = (densities[j] - 0.52f) * 2.15f;
    const float probability = std::min(root * root, 1.0f);
    // Six binomial standard deviations, plus endpoint rounding.
    const float tolerance = 6.0f * sqrtf(trials * probability *
        (1.0f - probability)) + 2.0f;
    CHECK(fabsf(accepted - trials * probability) <= tolerance);
  }
}

void TestClockDivisions() {
  test_checker.Start("TestClockDivisions");
  const int divisions[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
  for (int j = 0; j < 8; ++j) {
    TriggerGenerator generator;
    generator.Init(kSampleRate);
    const float density = (float(j) + 0.5f) / (8.0f * 2.15f);
    for (int i = 0; i < 48 * 2000; ++i) {
      const int tick = i / 2000;
      const int phase = i % 2000;
      GateFlags flags = phase == 0 ? GATE_FLAG_HIGH | GATE_FLAG_RISING
          : phase < 100 ? GATE_FLAG_HIGH : GATE_FLAG_LOW;
      TriggerGenerator::Output o;
      generator.Render(SEED_BUTTON_STATE_LOCKED, &flags, density,
          0, 0, &o, 1);
      CHECK(o.trigger == (phase == 0 && tick % divisions[j] == 0));
      CHECK(o.gate == (phase < 100 && tick % divisions[j] == 0));
      CHECK(o.regularity == 1.0f);
    }
  }
}

void TestSeedGateEnable() {
  test_checker.Start("TestSeedGateEnable");
  for (int button = 0; button < 2; ++button) {
    TriggerGenerator generator;
    generator.Init(kSampleRate);
    int count = 0;
    for (int i = 0; i < kSampleRate * 2; ++i) {
      const bool active = i >= 1000 && i < kSampleRate;
      GateFlags flags = active ? GATE_FLAG_HIGH : GATE_FLAG_LOW;
      TriggerGenerator::Output o;
      generator.Render(button && active ? SEED_BUTTON_STATE_ON
          : SEED_BUTTON_STATE_OFF, button ? NULL : &flags,
          0, 0, 0, &o, 1);
      CHECK(active || (!o.trigger && !o.gate));
      if (o.trigger) ++count;
    }
    CHECK(count > 120);
  }
}

void TestManualNoonEdges() {
  test_checker.Start("TestManualNoonEdges");
  for (int button = 0; button < 2; ++button) {
    TriggerGenerator generator;
    generator.Init(kSampleRate);
    int count = 0;
    for (int i = 0; i < 10000; ++i) {
      const bool active = i % 2000 >= 100 && i % 2000 < 1100;
      GateFlags flags = active ? GATE_FLAG_HIGH : GATE_FLAG_LOW;
      TriggerGenerator::Output o;
      generator.Render(button && active ? SEED_BUTTON_STATE_ON
          : SEED_BUTTON_STATE_OFF, button ? NULL : &flags,
          0.5f, 0, 0, &o, 1);
      CHECK(o.trigger == (i % 2000 == 100));
      CHECK(o.gate == active);
      if (o.trigger) ++count;
    }
    CHECK(count == 5);
  }
}

void TestExponentialTriggerFm() {
  test_checker.Start("TestExponentialTriggerFm");
  int shortest, longest;
  const int base = Count(SEED_BUTTON_STATE_LOCKED, 0, 0, 0,
      kSampleRate * 10, &shortest, &longest);
  const int octave = Count(SEED_BUTTON_STATE_LOCKED, 0, 0, 0.2f,
      kSampleRate * 10, &shortest, &longest);
  CHECK(std::abs(octave - base * 2) <= 2);
}

int main() {
  TestDensityModulation();
  TestLatchedNoonSilence();
  TestRegularDensityRate();
  TestRandomDensityRate();
  TestClockedNoonSilence();
  TestClockProbability();
  TestClockDivisions();
  TestSeedGateEnable();
  TestManualNoonEdges();
  TestExponentialTriggerFm();
  return test_checker.failures() ? 1 : 0;
}

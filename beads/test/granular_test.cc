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
// Grain controls and seeded attenurandomizers.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "beads/test/test_checker.h"
#include "beads/dsp/delay_granular_player.h"

// Exclude hardware declarations; exercise the real channel laws below.
#define BEADS_DRIVERS_CV_ADC_H_
#define BEADS_DRIVERS_POTS_ADC_H_
#define BEADS_SETTINGS_H_
namespace beads {
enum CvAdcChannel { CV_ADC_CHANNEL_LAST };
enum PotsAdcChannel { POTS_ADC_CHANNEL_LAST };
class Settings;
struct CvAdc {
  const uint16_t* values();
  float float_value(int) const;
};
struct PotsAdc { float float_value(int) const; };
}
#include "beads/cv_reader.h"
namespace beads {
PotChannel::PotChannel() { }
PotChannel::~PotChannel() { }
}
using namespace beads;

struct TaggedLine {
  enum { MAX_DELAY = 960000, NUM_CHANNELS = 2 };
  mutable float age;
  bool frozen() const { return true; }
  int duration() const { return MAX_DELAY; }
  template<int method>
  FloatFrame Read(int delay, float fraction) const {
    age = float(delay) + fraction;
    FloatFrame result = { age / MAX_DELAY, age / MAX_DELAY };
    return result;
  }
};

void TestGrainTimeAndDirection() {
  test_checker.Start("TestGrainTimeAndDirection");
  Parameters p = {};
  p.size.base = 0.5f;
  TriggerGenerator::Output trigger = {};
  trigger.trigger = true;
  trigger.regularity = 1.0f;
  TaggedLine line;
  float ages[2];
  for (int i = 0; i < 2; ++i) {
    DelayGranularPlayer player;
    player.Init(48000.0f);
    p.time.base = float(i);
    FloatFrame out = { 0.0f, 0.0f };
    player.Render(&line, trigger, p, &out);
    ages[i] = line.age;
    CHECK(ages[i] > 0.0f && ages[i] < TaggedLine::MAX_DELAY);
  }
  CHECK(ages[1] > ages[0] + 900000.0f);
  for (int i = 0; i < 2; ++i) {
    DelayGranularPlayer player;
    player.Init(48000.0f);
    p.time.base = 0.5f;
    p.size.base = i ? 0.5f : 0.0f;
    FloatFrame out = { 0.0f, 0.0f };
    player.Render(&line, trigger, p, &out);
    float first = line.age;
    trigger.trigger = false;
    player.Render(&line, trigger, p, &out);
    CHECK(line.age - first == (i ? -1.0f : 1.0f));
    trigger.trigger = true;
  }
  // Active grains retain their start parameters until another trigger.
  DelayGranularPlayer changed, reference;
  changed.Init(48000.0f);
  reference.Init(48000.0f);
  p.time.base = 0.5f;
  p.size.base = 0.5f;
  Parameters original = p;
  for (int i = 0; i < 100; ++i) {
    trigger.trigger = i == 0;
    FloatFrame a = { 0.0f, 0.0f };
    FloatFrame b = a;
    stmlib::Random::Seed(123);
    changed.Render(&line, trigger, p, &a);
    stmlib::Random::Seed(123);
    reference.Render(&line, trigger, original, &b);
    CHECK(a.l == b.l && a.r == b.r);
    p.time.base = 0.9f;
    p.size.base = 0.0f;
    p.pitch.base = 24.0f;
    p.shape.base = 1.0f;
  }

}

void TestGrainDuration() {
  test_checker.Start("TestGrainDuration");
  DelayGranularPlayer player;
  const float rates[] = { 24000.0f, 32000.0f, 48000.0f };
  for (int r = 0; r < 3; ++r) {
    player.Init(rates[r]);
    CHECK(fabsf(player.ComputeGrainDuration(0.0f) / rates[r] - 4.0f)
        < 0.001f);
    CHECK(fabsf(player.ComputeGrainDuration(1.0f) / rates[r] - 4.0f)
        < 0.001f);
    CHECK(fabsf(player.ComputeGrainDuration(1.0f / 3.0f) / rates[r]
        - 0.0078125f) < 0.00001f);
    float previous = 0.0f;
    for (int i = 0; i <= 100; ++i) {
      float duration = player.ComputeGrainDuration(
          1.0f / 3.0f + float(i) / 150.0f);
      CHECK(duration >= previous);
      previous = duration;
    }
    previous = 0.0f;
    for (int i = 100; i >= 0; --i) {
      float duration = player.ComputeGrainDuration(float(i) / 300.0f);
      CHECK(duration >= previous);
      previous = duration;
    }
  }
}

void TestGrainShape() {
  test_checker.Start("TestGrainShape");
  for (int direction = -1; direction <= 1; direction += 2) {
    TaggedLine line;
    int attacks[2] = { 0, 0 };
    for (int shape = 0; shape < 2; ++shape) {
      DelayGrain grain;
      grain.Init(48000.0f);
      grain.Start(&line, 480000.0f, float(direction), 4800.0f,
          shape ? 1.0f : 2.0f / 3.0f, 1.0f, 0.5f, 0.0f);
      float peak = 0.0f;
      for (int i = 0; i < 6000; ++i) {
        FloatFrame out = { 0.0f, 0.0f };
        grain.Render<HEAD_CROSSING_POLICY_STOP, false>(&line, &out);
        float envelope = out.l / (line.age / TaggedLine::MAX_DELAY);
        if (envelope > peak) { peak = envelope; attacks[shape] = i; }
      }
      CHECK(grain.done());
    }
    CHECK(attacks[0] > 2350 && attacks[0] < 2450);
    CHECK(attacks[1] > 4750 && attacks[1] < 4850);
  }
}

void TestPitchNotches() {
  test_checker.Start("TestPitchNotches");
  PotChannel pot;
  pot.Init(PotChannel::LAW_DETUNE, 1.0f, 0.0f);
  const float intervals[] = { 0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f };
  for (int sign = -1; sign <= 1; sign += 2) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 1; j <= 2; ++j) {
        pot.Process(uint16_t(32768.0f *
            (1.0f + sign * (float(i) + float(j) * 0.1f) / 6.0f)));
        CHECK(fabsf(pot.value() - sign * intervals[i]) < 0.00001f);
      }
    }
  }
  pot.Process(0); CHECK(pot.value() == -24.0f);
  pot.Process(65535); CHECK(fabsf(pot.value() - 24.0f) < 0.001f);
}

void TestPatchedAndNoon() {
  test_checker.Start("TestPatchedAndNoon");
  RandomizedParameter p = { 0.25f, 0.0f, 0.5f, true };
  for (int i = 0; i <= 4; ++i) {
    p.attenuverter = float(i) / 4.0f;
    CHECK(p.value() == 0.25f + float(i) / 8.0f);
  }
  p.attenuverter = 0.0f;
  for (int patched = 0; patched < 2; ++patched) {
    p.has_cv = patched;
    for (int i = 0; i < 100; ++i) {
      stmlib::Random::Seed(i + 1);
      p.cv = float(i) - 50.0f;
      CHECK(p.value() == p.base);
    }
  }
  p.has_cv = true; p.base = 0.0f; p.attenuverter = 1.0f;
  p.cv = 0.0f; float rate = p.pitch_value();
  p.cv = 12.0f; CHECK(fabsf(p.pitch_value() / rate - 2.0f) < 0.001f);
  for (int sign = -1; sign <= 1; sign += 2) {
    p.has_cv = false; p.attenuverter = float(sign); p.base = 0.0f;
    stmlib::Random::Seed(123); rate = p.pitch_value();
    p.base = 12.0f;
    stmlib::Random::Seed(123);
    CHECK(fabsf(p.pitch_value() / rate - 2.0f) < 0.001f);
  }
}

void TestPatchedRandomDepth() {
  test_checker.Start("TestPatchedRandomDepth");
  stmlib::Random::Seed(456);
  RandomizedParameter p = { 0.5f, -0.5f, -1.0f, true };
  for (int i = 0; i < 100; ++i) CHECK(p.value() == 0.5f);
  float spread = 0.0f;
  for (int i = 1; i <= 100; ++i) {
    stmlib::Random::Seed(i);
    p.cv = 0.25f; float small = p.value() - p.base;
    stmlib::Random::Seed(i);
    p.cv = 0.5f; float large = p.value() - p.base;
    CHECK(fabsf(large - 2.0f * small) < 0.000001f);
    CHECK(fabsf(large) <= 0.25f);
    spread = std::max(spread, fabsf(large));
  }
  // The peaky distribution exceeds 0.2 (0.05 here) on about 60% of draws.
  CHECK(spread > 0.05f);
}

void TestInternalDistributions() {
  test_checker.Start("TestInternalDistributions");
  const int kDraws = 40000;
  int bins[10] = {};
  int centres[2] = { 0, 0 };
  for (int mode = 0; mode < 2; ++mode) {
    RandomizedParameter p = { 0.5f, mode ? -0.25f : 0.25f, 0.0f, false };
    stmlib::Random::Seed(0x12345678);
    for (int i = 0; i < kDraws; ++i) {
      float v = p.value(NULL, 0, 1.0f, false) - p.base;
      CHECK(fabsf(v) <= (mode ? 0.75f : 0.25f));
      if (fabsf(v) < (mode ? 0.075f : 0.025f)) ++centres[mode];
      if (!mode) ++bins[std::min(int((v + 0.25f) * 20.0f), 9)];
    }
  }
  // Uniform bins expect 4000; tolerance exceeds six standard deviations.
  for (int i = 0; i < 10; ++i) CHECK(bins[i] > 3600 && bins[i] < 4400);
  CHECK(centres[0] > 3600 && centres[0] < 4400);
  CHECK(centres[1] > 11000 && centres[1] < 16000);
}

void TestAttenurandomizerLaw() {
  test_checker.Start("TestAttenurandomizerLaw");
  PotChannel pot;
  pot.Init(PotChannel::LAW_ATTENUVERTER, 1.0f, 0.0f);
  float previous = -2.0f;
  for (int raw = 0; raw <= 65535; raw += 257) {
    pot.Process(raw);
    CHECK(pot.value() >= previous);
    if (raw >= 30802 && raw <= 34734) CHECK(pot.value() == 0.0f);
    previous = pot.value();
  }
  pot.Process(0); CHECK(pot.value() == -1.0f);
  pot.Process(65535); CHECK(pot.value() > 0.999f);
  pot.Process(49152);
  CHECK(fabsf(pot.value() - 0.23404255f) < 0.000001f);
}

void TestSharedRandomStream() {
  test_checker.Start("TestSharedRandomStream");
  RandomizedParameter a = { 0.5f, 1.0f, 0.0f, false };
  RandomizedParameter b = { 0.5f, 1.0f, 0.0f, false };
  stmlib::Random::Seed(123);
  float first = a.value();
  float second = b.value();
  stmlib::Random::Seed(123);
  CHECK(b.value() == first);
  CHECK(a.value() == second);
  CHECK(first != second);
}

int main() {
  TestGrainTimeAndDirection();
  TestGrainDuration();
  TestGrainShape();
  TestPitchNotches();
  TestPatchedAndNoon();
  TestPatchedRandomDepth();
  TestInternalDistributions();
  TestAttenurandomizerLaw();
  TestSharedRandomStream();
  return test_checker.failures() ? 1 : 0;
}

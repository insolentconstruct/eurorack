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
#include <stdint.h>

#include "beads/test/test_checker.h"

#define private public
#define protected public
#include "beads/dsp/processor.h"
#undef protected
#undef private

// ADC transfers are inert; channel filtering and calibration are firmware.
struct ADC_HandleTypeDef { };
struct DMA_HandleTypeDef { };
struct TIM_HandleTypeDef { };
#define BEADS_TEST_OPTIONS_STUBS_BEADS_DRIVERS_CV_ADC_H_
#define private public
#include "../drivers/cv_adc.h"
#include "beads/cv_reader.h"
#undef private
#include "beads/cv_reader.cc"

namespace beads {

uint16_t CvAdc::values_[CV_ADC_CHANNEL_LAST];
uint16_t PotsAdc::values_[POTS_ADC_CHANNEL_LAST];
bool CvAdc::Init() { return true; }
void CvAdc::Convert() { }
bool PotsAdc::Init() { return true; }
void PotsAdc::Convert() { }
void PotsAdc::StartMuxedPotsConversion() { }
CvChannel::CvChannel() { }
CvChannel::~CvChannel() { }
PotChannel::PotChannel() { }
PotChannel::~PotChannel() { }

}  // namespace beads

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
  Parameters p = { };
  p.size.base = 0.5f;
  TriggerGenerator::Output trigger = { };
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
      const float duration = player.ComputeGrainDuration(
          1.0f / 3.0f + float(i) / 150.0f);
      CHECK(duration >= previous);
      previous = duration;
    }
    previous = 0.0f;
    for (int i = 100; i >= 0; --i) {
      const float duration = player.ComputeGrainDuration(float(i) / 300.0f);
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
        if (envelope > peak) {
          peak = envelope;
          attacks[shape] = i;
        }
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
  pot.Process(0);
  CHECK(pot.value() == -24.0f);
  pot.Process(65535);
  CHECK(fabsf(pot.value() - 24.0f) < 0.001f);
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
  p.has_cv = true;
  p.base = 0.0f;
  p.attenuverter = 1.0f;
  p.cv = 0.0f;
  float rate = p.pitch_value();
  p.cv = 12.0f;
  CHECK(fabsf(p.pitch_value() / rate - 2.0f) < 0.001f);
  for (int sign = -1; sign <= 1; sign += 2) {
    p.has_cv = false;
    p.attenuverter = float(sign);
    p.base = 0.0f;
    stmlib::Random::Seed(123);
    rate = p.pitch_value();
    p.base = 12.0f;
    stmlib::Random::Seed(123);
    CHECK(fabsf(p.pitch_value() / rate - 2.0f) < 0.001f);
  }
}

void TestPatchedRandomDepth() {
  test_checker.Start("TestPatchedRandomDepth");
  stmlib::Random::Seed(456);
  RandomizedParameter p = { 0.5f, -0.5f, -1.0f, true };
  for (int i = 0; i < 100; ++i) {
    CHECK(p.value() == 0.5f);
  }
  float spread = 0.0f;
  for (int i = 1; i <= 100; ++i) {
    stmlib::Random::Seed(i);
    p.cv = 0.25f;
    float small = p.value() - p.base;
    stmlib::Random::Seed(i);
    p.cv = 0.5f;
    float large = p.value() - p.base;
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
  int bins[10] = { };
  int centres[2] = { 0, 0 };
  for (int mode = 0; mode < 2; ++mode) {
    RandomizedParameter p = { 0.5f, mode ? -0.25f : 0.25f, 0.0f, false };
    stmlib::Random::Seed(0x12345678);
    for (int i = 0; i < kDraws; ++i) {
      float v = p.value(NULL, 0, 1.0f, false) - p.base;
      CHECK(fabsf(v) <= (mode ? 0.75f : 0.25f));
      if (fabsf(v) < (mode ? 0.075f : 0.025f)) {
        ++centres[mode];
      }
      if (!mode) {
        ++bins[std::min(int((v + 0.25f) * 20.0f), 9)];
      }
    }
  }
  // Uniform bins expect 4000; tolerance exceeds six standard deviations.
  for (int i = 0; i < 10; ++i) {
    CHECK(bins[i] > 3600 && bins[i] < 4400);
  }
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
    if (raw >= 30802 && raw <= 34734) {
      CHECK(pot.value() == 0.0f);
    }
    previous = pot.value();
  }
  pot.Process(0);
  CHECK(pot.value() == -1.0f);
  pot.Process(65535);
  CHECK(pot.value() > 0.999f);
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

// Real Processor path with a frozen, nonzero digital recording.
static Processor processor;
static uint8_t banks[3][256 * 1024];
static uint16_t reverb_buffer[32768];
static Parameters parameters;

void InitProcessor() {
  uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
  processor.Init(buffers, reverb_buffer);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  parameters = Parameters();
  parameters.time.base = 0.5f;
  parameters.size.base = 0.8f;
  parameters.shape.base = 0.0f;
  int16_t* samples = reinterpret_cast<int16_t*>(banks);
  for (size_t i = 0; i < sizeof(banks) / sizeof(int16_t); ++i) {
    samples[i] = 4096;
  }
  stmlib::Random::Seed(12345);
}

FloatFrame ProcessSample(
    bool trigger, float density_base = 0.48f, bool external = true) {
  const bool freeze = true;
  const stmlib::GateFlags gate = trigger
      ? static_cast<stmlib::GateFlags>(
          stmlib::GATE_FLAG_RISING | stmlib::GATE_FLAG_HIGH)
      : stmlib::GATE_FLAG_LOW;
  ModulatedParameter density = { density_base, 0.0f };
  Processor::MixParameters mix = { 0.0f, 1.0f, 0.0f, false };
  FloatFrame input = { 0.0f, 0.0f };
  FloatFrame output;
  processor.Process(
      external ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_ON,
      external ? &gate : NULL,
      &freeze, density,
      parameters, mix, &input, &output, 1);
  return output;
}

DelayGranularPlayer& Player() {
  return processor.processor_digital_.granular_player_;
}

void TestMonophonicGrains() {
  test_checker.Start("TestMonophonicGrains");
  for (int interval = 48; interval <= 480; interval *= 10) {
    DelayGranularPlayer player;
    player.Init(48000.0f);
    TaggedLine line;
    for (int j = 0; j < 32; ++j) {
      player.grain_[j].Start(&line, 480000.0f, 1.0f, 48000.0f,
          0.0f, 1.0f, 0.5f, 0.0f);
    }
    player.grain_interval_ = interval;
    DelayGrain* candidate = player.grain_priority_list_[1];
    DelayGrain* fresh = player.grain_priority_list_[0];
    player.SortGrainList(true);
    CHECK(candidate->remaining_samples() == std::min(interval, 240));
    CHECK(fresh->remaining_samples() > 240);
    for (int j = 1; j < 31; ++j) {
      CHECK(player.grain_priority_list_[j]->remaining_samples() == 240);
    }
  }
  for (int mono = 0; mono < 2; ++mono) {
    for (int interval = 48; interval <= 480; interval *= 10) {
      InitProcessor();
      // Fill the priority list before switching modes.
      for (int i = 0; i < 32; ++i) {
        ProcessSample(true);
        ProcessSample(false);
      }
      for (int i = 0; i < interval; ++i) {
        ProcessSample(false);
      }
      DelayGrain* candidate = Player().grain_priority_list_[1];
      const int elapsed = Player().grain_interval_;
      parameters.monophonic_mode = mono;
      ProcessSample(true);
      if (mono) {
        CHECK(candidate->remaining_samples() <= std::min(elapsed, 240));
        for (int j = 0; j < 31; ++j) {
          CHECK(Player().grain_priority_list_[j]->remaining_samples() <= 240);
        }
      }
      for (int i = 0; i < 241; ++i) {
        ProcessSample(false);
      }
      int active = 0;
      for (int j = 0; j < 32; ++j) {
        active += !Player().grain_[j].done();
      }
      CHECK(mono ? active == 1 : active > 20);
    }
  }
  for (int shape = 0; shape < 2; ++shape) {
    InitProcessor();
    parameters.monophonic_mode = true;
    parameters.shape.base = float(shape) / 3.0f;
    float peak = 0.0f;
    for (int i = 0; i < 9600; ++i) {
      FloatFrame output = ProcessSample(i % 2400 == 0);
      peak = std::max(peak, fabsf(output.l));
    }
    CHECK(peak > 0.01f);
  }
}

void TestProcessorGrainLatch() {
  test_checker.Start("TestProcessorGrainLatch");
  InitProcessor();
  ProcessSample(true);
  DelayGrain& grain = Player().grain_[0];
  const int speed = grain.speed_;
  const int initial_delay = grain.initial_delay_;
  const float attack = grain.envelope_.phase_increment_[0];
  const float release = grain.envelope_.phase_increment_[2];
  const int remaining = grain.remaining_samples();
  CHECK(!grain.done());
  for (int i = 0; i < 100; ++i) {
    parameters.time.base = 0.9f;
    parameters.size.base = 0.0f;
    parameters.pitch.base = 12.0f;
    parameters.shape.base = 1.0f;
    parameters.time.cv = 0.3f;
    parameters.time.has_cv = true;
    parameters.time.attenuverter = 1.0f;
    ProcessSample(false);
    CHECK(grain.speed_ == speed);
    CHECK(grain.initial_delay_ == initial_delay);
    CHECK(grain.envelope_.phase_increment_[0] == attack);
    CHECK(grain.envelope_.phase_increment_[2] == release);
  }
  CHECK(grain.remaining_samples() == remaining - 100);
  ProcessSample(true);
  DelayGrain& next = Player().grain_[1];
  CHECK(!next.done());
  CHECK(next.speed_ == -2 * speed);
  CHECK(next.initial_delay_ != initial_delay);
  CHECK(next.envelope_.phase_increment_[0] != attack);
}

void TestProcessorEnvelopeCompletion() {
  test_checker.Start("TestProcessorEnvelopeCompletion");
  InitProcessor();
  parameters.size.base = 1.0f / 3.0f;
  parameters.shape.base = 2.0f / 3.0f;
  ProcessSample(true);
  DelayGrain& first = Player().grain_[0];
  float peak = 0.0f;
  for (int i = 0; i < 1000; ++i) {
    FloatFrame output = ProcessSample(false);
    peak = std::max(peak, output.l);
  }
  CHECK(peak > 0.01f);
  CHECK(first.envelope_.done());
  CHECK(first.done());
  CHECK(first.remaining_samples() == 0);
  for (int i = 0; i < 100; ++i) {
    FloatFrame output = ProcessSample(false);
    CHECK(output.l == 0.0f && output.r == 0.0f);
  }
  ProcessSample(true);
  CHECK(!Player().grain_[1].done());
  CHECK(first.done());
}

void TestProcessorVoiceRelease() {
  test_checker.Start("TestProcessorVoiceRelease");
  InitProcessor();
  for (int i = 0; i < 20; ++i) {
    ProcessSample(i == 0);
  }
  DelayGrain& grain = Player().grain_[0];
  CHECK(grain.voice_stealing_envelope_.value_ == 1.0f);
  // A full rotation brings the sounding voice into the safety set.
  for (int i = 0; i < 28; ++i) {
    ProcessSample(true);
    ProcessSample(false);
  }
  CHECK(grain.voice_stealing_envelope_.decrement_ > 0.0f);
  float previous = grain.voice_stealing_envelope_.value_;
  CHECK(previous > 0.0f && previous < 1.0f);
  for (int i = 0; i < 10; ++i) {
    FloatFrame output = ProcessSample(false);
    const float value = grain.voice_stealing_envelope_.value_;
    CHECK(value > 0.0f && value < previous);
    CHECK(output.l > 0.0f);
    previous = value;
  }
  for (int i = 0; i < 5000; ++i) {
    ProcessSample(false);
  }
  CHECK(grain.done());
}

void TestProcessorVoiceSaturation() {
  test_checker.Start("TestProcessorVoiceSaturation");
  InitProcessor();
  int max_musical = 0;
  int max_safety = 0;
  int max_active = 0;
  for (int i = 0; i < 20000; ++i) {
    ProcessSample(i % 240 == 0);
    int musical = 0;
    int safety = 0;
    for (int j = 0; j < Player().NUM_GRAINS; ++j) {
      DelayGrain& grain = Player().grain_[j];
      if (!grain.done()) {
        if (grain.voice_stealing_envelope_.decrement_ > 0.0f) {
          ++safety;
        } else {
          ++musical;
        }
      }
    }
    max_musical = std::max(max_musical, musical);
    max_safety = std::max(max_safety, safety);
    max_active = std::max(max_active, musical + safety);
  }
  printf("Voice maxima: musical=%d safety=%d active=%d\n",
      max_musical, max_safety, max_active);
  CHECK(Player().NUM_GRAINS == 32);
  CHECK(max_musical == 28);
  CHECK(max_safety >= 2);
  CHECK(max_active >= 30 && max_active <= 32);
}

void TestProcessorRandomPan() {
  test_checker.Start("TestProcessorRandomPan");
  for (int mode = 0; mode < 5; ++mode) {
    InitProcessor();
    RandomizedParameter* controls[] = {
      &parameters.time, &parameters.size,
      &parameters.shape, &parameters.pitch
    };
    if (mode >= 1 && mode <= 4) {
      controls[mode - 1]->attenuverter = 0.1f;
    }
    float minimum = 10.0f;
    float maximum = -10.0f;
    for (int i = 0; i < 20; ++i) {
      ProcessSample(true);
      DelayGrain& grain = Player().grain_[i];
      float balance = grain.gain_[0][0] - grain.gain_[1][1];
      minimum = std::min(minimum, balance);
      maximum = std::max(maximum, balance);
      ProcessSample(false);
    }
    if (mode == 0) {
      CHECK(minimum == 0.0f && maximum == 0.0f);
    } else {
      CHECK(maximum - minimum > 0.01f);
    }
  }
  InitProcessor();
  float minimum = 10.0f;
  float maximum = -10.0f;
  for (int i = 0; i < 24000; ++i) {
    ProcessSample(false, 0.9f, false);
    for (int j = 0; j < Player().NUM_GRAINS; ++j) {
      DelayGrain& grain = Player().grain_[j];
      if (!grain.done()) {
        float balance = grain.gain_[0][0] - grain.gain_[1][1];
        minimum = std::min(minimum, balance);
        maximum = std::max(maximum, balance);
      }
    }
  }
  CHECK(maximum - minimum > 0.01f);
}

void TestPitchCvImmediate() {
  test_checker.Start("TestPitchCvImmediate");
  Settings settings;
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    settings.mutable_persistent_data()->adc_calibration[i].scale = 120.0f;
    settings.mutable_persistent_data()->adc_calibration[i].offset = -60.0f;
  }
  CvReader reader;
  reader.Init(&settings);
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    reader.cv_.values_[i] = 32768;
  }
  reader.Read();
  CHECK(reader.cv(CV_ADC_CHANNEL_PITCH) == 0.0f);
  reader.cv_.values_[CV_ADC_CHANNEL_PITCH] = 39322;
  reader.Read();
  const float expected = 120.0f * (39322.0f / 65536.0f) - 60.0f;
  CHECK(reader.cv(CV_ADC_CHANNEL_PITCH) == expected);
  InitProcessor();
  parameters.pitch.has_cv = true;
  parameters.pitch.attenuverter = 1.0f;
  parameters.pitch.cv = reader.cv(CV_ADC_CHANNEL_PITCH);
  ProcessSample(true);
  // The replay increment is quantized to 1/1024 sample.
  CHECK(abs(Player().grain_[0].speed_ - 2048) <= 1);
  reader.cv_.values_[CV_ADC_CHANNEL_PITCH] = 32768;
  reader.Read();
  CHECK(reader.cv(CV_ADC_CHANNEL_PITCH) == 0.0f);
}

void TestCvChannelIsolation() {
  test_checker.Start("TestCvChannelIsolation");
  Settings settings;
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    settings.mutable_persistent_data()->adc_calibration[i].scale = 2.0f;
    settings.mutable_persistent_data()->adc_calibration[i].offset = -1.0f;
  }
  const CvAdcChannel channels[] = {
    CV_ADC_CHANNEL_TIME, CV_ADC_CHANNEL_SIZE,
    CV_ADC_CHANNEL_SHAPE, CV_ADC_CHANNEL_PITCH
  };
  for (int changed = 0; changed < 4; ++changed) {
    CvReader reader;
    reader.Init(&settings);
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      reader.cv_.values_[i] = 32768;
    }
    reader.Read();
    reader.cv_.values_[channels[changed]] = 49152;
    reader.Read();
    for (int i = 0; i < 4; ++i) {
      const float expected = i == changed
          ? (channels[i] == CV_ADC_CHANNEL_PITCH ? 0.5f : 0.25f)
          : 0.0f;
      CHECK(reader.cv(channels[i]) == expected);
    }
  }
}

void ProcessQuantizedBlock(bool trigger) {
  bool freeze[4];
  stmlib::GateFlags gates[4];
  FloatFrame in[4];
  FloatFrame out[4];
  for (int i = 0; i < 4; ++i) {
    freeze[i] = true;
    gates[i] = trigger && i == 0
        ? stmlib::GateFlags(stmlib::GATE_FLAG_RISING | stmlib::GATE_FLAG_HIGH)
        : stmlib::GATE_FLAG_LOW;
    in[i].l = 0.0f;
    in[i].r = 0.0f;
  }
  ModulatedParameter density;
  density.base = 0.48f;
  density.cv = 0.0f;
  Processor::MixParameters mix;
  mix.feedback = 0.0f;
  mix.dry_wet = 1.0f;
  mix.reverb_amount = 0.0f;
  mix.output_gate = false;
  processor.Process(SEED_BUTTON_STATE_OFF, gates, freeze, density,
      parameters, mix, in, out, 4);
}

void TestQuantizedGrainPitch() {
  test_checker.Start("TestQuantizedGrainPitch");
  for (int quality = 0; quality < 4; ++quality) {
    for (int mono = 0; mono < 2; ++mono) {
      for (int reverse = 0; reverse < 2; ++reverse) {
        InitProcessor();
        processor.Configure(Processor::Media(quality), mono);
        parameters.size.base = reverse ? 0.2f : 0.8f;
        parameters.pitch.base = 1.4f;
        parameters.pitch.cv = 3.0f;
        parameters.pitch.has_cv = true;
        parameters.pitch.attenuverter = 1.0f;
        processor.set_pitch_scale(PITCH_SCALE_FIFTHS);
        ProcessQuantizedBlock(true);
        DelayGranularPlayer* player = quality == 0
            ? &processor.processor_dirty_tape_.granular_player_
            : quality == 1 ? &processor.processor_clean_tape_.granular_player_
            : quality == 2 ? &processor.processor_vintage_digital_.granular_player_
            : &processor.processor_digital_.granular_player_;
        DelayGrain& grain = player->grain_[0];
        const int speed = grain.speed_;
        const int expected = int(stmlib::SemitonesToRatio(7.0f) * 1024.0f);
        CHECK(abs(abs(speed) - expected) <= 1);
        CHECK((speed < 0) == bool(reverse));
        processor.set_pitch_scale(PITCH_SCALE_CHROMATIC);
        for (int i = 0; i < 20; ++i) {
          ProcessQuantizedBlock(false);
          CHECK(grain.speed_ == speed);
        }
      }
    }
  }
}

int main() {
  TestMonophonicGrains();
  TestProcessorGrainLatch();
  TestQuantizedGrainPitch();
  TestProcessorEnvelopeCompletion();
  TestProcessorVoiceRelease();
  TestProcessorVoiceSaturation();
  TestProcessorRandomPan();
  TestPitchCvImmediate();
  TestCvChannelIsolation();
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

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
// Processor delay playback, transitions, pitch and bandwidth tests.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/processor.h"
#undef private

using namespace beads;

struct Rig {
  uint8_t banks[3][256 * 1024];
  uint16_t reverb[32768];
  Processor processor;
  Parameters parameters;
  ModulatedParameter density;
  Processor::MixParameters mix;
  int sample;
  float frequency;

  Rig() : sample(0), frequency(750.0f) {
    memset(banks, 0, sizeof(banks));
    memset(reverb, 0, sizeof(reverb));
    memset(&parameters, 0, sizeof(parameters));
    memset(&density, 0, sizeof(density));
    memset(&mix, 0, sizeof(mix));
    uint8_t* buffers[3] = { banks[0], banks[1], banks[2] };
    processor.Init(buffers, reverb);
    parameters.size.base = 65535.0f / 65536.0f;
    parameters.shape.base = 0.5f;
    density.base = 0.85f;
    mix.dry_wet = 1.0f;
  }

  void Block(std::vector<float>* capture = NULL) {
    const int size = int(processor.sample_rate()) / 4000;
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12] = { };
    for (int i = 0; i < size; ++i) {
      const float x = 0.05f * sin(
          6.283185307179586 * frequency *
          (sample++ % int(processor.sample_rate())) / processor.sample_rate());
      in[i].l = x;
      in[i].r = x;
    }
    processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, density,
        parameters, mix, in, out, size);
    if (capture) {
      for (int i = 0; i < size; ++i) {
        capture->push_back(out[i].l);
      }
    }
  }

  void Run(int blocks) {
    while (blocks--) {
      Block();
    }
  }
};

float Rms(const std::vector<float>& audio) {
  double energy = 0.0;
  for (size_t i = 0; i < audio.size(); ++i) {
    energy += audio[i] * audio[i];
  }
  return sqrt(energy / audio.size());
}

void TestContinuousDelayPlayback() {
  test_checker.Start("TestContinuousDelayPlayback");
  Rig r;
  r.processor.Configure(Processor::MEDIA_DIRTY_TAPE, false);
  r.Run(14000);
  CHECK(r.processor.delay_mode_transition_ > 1.49f);
  const int before = *r.processor.mutable_delay_write_ptr();
  for (int second = 0; second < 3; ++second) {
    std::vector<float> audio;
    for (int block = 0; block < 4000; ++block) {
      r.Block(&audio);
    }
    // 0.05 input leaves ample margin over quantization noise.
    CHECK(Rms(audio) > 0.02f);
    CHECK(r.processor.delay_mode_transition_ > 1.49f);
  }
  CHECK(*r.processor.mutable_delay_write_ptr() != before);
}

void TestDelayModeFade() {
  test_checker.Start("TestDelayModeFade");
  Rig r;
  for (int direction = 0; direction < 2; ++direction) {
    r.parameters.size.base = direction ? 0.5f : 1.0f;
    const float checkpoints[] = {
      -1.5f, -0.75f, -0.375f, -0.00025f, 0.0f, 0.375f, 0.75f, 1.5f
    };
    float minimum = 1.5f;
    for (int point = 0; point < 8; ++point) {
      // Each checkpoint is reachable by holding SIZE in the chosen direction.
      const float before = checkpoints[direction ? 7 - point : point];
      r.processor.delay_mode_transition_ = before;
      r.Block();
      const float t = r.processor.delay_mode_transition_;
      const float next = before + (direction ? -0.00025f : 0.00025f);
      CHECK(fabsf(t - std::max(-1.5f, std::min(1.5f, next))) < 0.000001f);
      // Block interpolation allows 2e-5 gain error at the target endpoint.
      const float x = std::min(1.0f, fabsf(t) / 0.75f);
      const float expected = 1.5f * x * x * (3.0f - 2.0f * x);
      CHECK(fabsf(r.processor.wet_ - expected) < 0.00002f);
      minimum = std::min(minimum, r.processor.wet_);
    }
    CHECK(minimum < 0.000001f);
    CHECK(r.processor.wet_ > 1.499f);
  }
}

float TonePower(const std::vector<float>& audio, float frequency) {
  double real = 0.0;
  double imaginary = 0.0;
  for (size_t i = 0; i < audio.size(); ++i) {
    const double phase = 6.283185307179586 * frequency * i / 48000.0;
    real += audio[i] * cos(phase);
    imaginary += audio[i] * sin(phase);
  }
  return (real * real + imaginary * imaginary) /
      (audio.size() * double(audio.size()));
}

void TestDelayRotaryPitch() {
  test_checker.Start("TestDelayRotaryPitch");
  Rig r;
  // The transition is tested separately; start from its settled endpoint.
  r.processor.delay_mode_transition_ = 1.5f;
  r.Run(2000);
  std::vector<float> unity;
  for (int block = 0; block < 1000; ++block) {
    r.Block(&unity);
  }
  CHECK(fabsf(r.processor.processor_digital_.speed_ - 1.0f) < 0.0001f);
  CHECK(TonePower(unity, 750.0f) > 0.0001f);
  r.parameters.pitch.base = 12.0f;
  r.Run(2000);
  std::vector<float> shifted;
  for (int block = 0; block < 1000; ++block) {
    r.Block(&shifted);
  }
  // Rotary windows add sidebands; the octave must dominate the original.
  CHECK(TonePower(shifted, 1500.0f) >
      10.0f * TonePower(shifted, 750.0f));
  CHECK(TonePower(shifted, 1500.0f) > 0.00001f);
  r.parameters.pitch.base = 0.0f;
  r.Run(2000);
  // Re-render identical state with different rotary positions at unity speed.
  std::vector<uint8_t> saved(sizeof(r));
  memcpy(&saved[0], &r, sizeof(r));
  std::vector<float> bypassed;
  r.Block(&bypassed);
  memcpy(static_cast<void*>(&r), &saved[0], sizeof(r));
  for (int i = 0; i < DelayController::NUM_TAPS; ++i) {
    r.processor.processor_digital_.multitap_delay_.tap_[i].phase_ = 1000000;
  }
  std::vector<float> moved;
  r.Block(&moved);
  // A bypass must be independent of the rotary heads' stored phases.
  CHECK(bypassed == moved);
}

float BandwidthRms(float frequency, float attenuverter, float cv,
    bool patched) {
  Rig r;
  r.frequency = frequency;
  r.parameters.size.attenuverter = attenuverter;
  r.parameters.size.cv = cv;
  r.parameters.size.has_cv = patched;
  r.processor.delay_mode_transition_ = 1.5f;
  r.Run(1500);
  std::vector<float> audio;
  for (int block = 0; block < 500; ++block) {
    r.Block(&audio);
  }
  return Rms(audio);
}

void TestDelayBandwidthAndCv() {
  test_checker.Start("TestDelayBandwidthAndCv");
  const float full_high = BandwidthRms(6000.0f, 0.0f, 0.0f, false);
  const float full_low = BandwidthRms(100.0f, 0.0f, 0.0f, false);
  const float dark = BandwidthRms(6000.0f, -1.0f, 0.0f, false);
  const float thin = BandwidthRms(100.0f, 1.0f, 0.0f, false);
  const float cv_dark = BandwidthRms(6000.0f, 0.0f, -0.5f, true);
  const float cv_thin = BandwidthRms(100.0f, 0.0f, 0.5f, true);
  // End-stop filters attenuate these tones by much more than 20 dB.
  CHECK(full_high > 0.02f);
  CHECK(full_low > 0.02f);
  CHECK(dark < full_high * 0.1f);
  CHECK(thin < full_low * 0.1f);
  CHECK(cv_dark < full_high * 0.1f);
  CHECK(cv_thin < full_low * 0.1f);
  // CV is additive even with the attenuverter at noon.
  CHECK(fabsf(cv_dark - dark) < 0.00001f);
  CHECK(fabsf(cv_thin - thin) < 0.00001f);
  CHECK(fabsf(BandwidthRms(6000.0f, 0.0f, -0.5f, false) - full_high)
      < 0.00001f);
}

void TestQuantizerDelayBypass() {
  test_checker.Start("TestQuantizerDelayBypass");
  for (int quality = 0; quality < 4; ++quality) {
    for (int mono = 0; mono < 2; ++mono) {
      Rig off;
      Rig on;
      off.processor.Configure(Processor::Media(quality), mono);
      on.processor.Configure(Processor::Media(quality), mono);
      off.parameters.pitch.base = 3.4f;
      on.parameters.pitch.base = 3.4f;
      off.parameters.pitch.attenuverter = 0.8f;
      on.parameters.pitch.attenuverter = 0.8f;
      stmlib::Random::Seed(123);
      off.Run(16000);
      stmlib::Random::Seed(123);
      on.Run(16000);
      CHECK(off.processor.delay_mode_transition_ > 1.49f);
      CHECK(on.processor.delay_mode_transition_ > 1.49f);
      on.processor.set_pitch_scale(PITCH_SCALE_PELOG);
      std::vector<float> expected;
      std::vector<float> actual;
      stmlib::Random::Seed(123);
      for (int i = 0; i < 1200; ++i) {
        off.Block(&expected);
      }
      stmlib::Random::Seed(123);
      for (int i = 0; i < 1200; ++i) {
        on.Block(&actual);
      }
      CHECK(expected == actual);
    }
  }
}

void TestDelayOptions() {
  test_checker.Start("TestDelayOptions");
  CHECK(DelayFilterDetent(0.0f) == 0.0f);
  CHECK(fabsf(DelayFilterDetent(1.0f) - 1.0f) < 0.000001f);
  for (int i = 400; i <= 600; ++i) {
    CHECK(DelayFilterDetent(float(i) / 1000.0f) == 0.5f);
  }
  for (int quality = 0; quality < 4; ++quality) {
    for (int mono = 0; mono < 2; ++mono) {
      Rig r;
      r.processor.Configure(Processor::Media(quality), mono);
      r.processor.set_delay_mode(DELAY_MODE_NEVER);
      r.Run(14000);
      CHECK(r.processor.delay_mode_transition_ < -1.49f);
      r.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
      r.Run(14000);
      CHECK(r.processor.delay_mode_transition_ > 1.49f);
      r.processor.set_delay_mode(DELAY_MODE_NEVER);
      r.Run(14000);
      CHECK(r.processor.delay_mode_transition_ < -1.49f);
    }
  }
  // The detent is a true bypass, including additive SIZE CV.
  Rig a;
  Rig b;
  a.processor.Configure(Processor::MEDIA_DIGITAL, false);
  b.processor.Configure(Processor::MEDIA_DIGITAL, false);
  b.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
  b.parameters.size.attenuverter = 0.1f;
  b.parameters.size.has_cv = true;
  b.parameters.size.cv = 0.04f;
  a.Run(14000);
  b.Run(14000);
  std::vector<float> stock;
  std::vector<float> detent;
  for (int i = 0; i < 1000; ++i) {
    a.Block(&stock);
    b.Block(&detent);
  }
  CHECK(stock == detent);
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  TestDelayOptions();
  TestContinuousDelayPlayback();
  TestQuantizerDelayBypass();
  TestDelayModeFade();
  TestDelayRotaryPitch();
  TestDelayBandwidthAndCv();
  return test_checker.failures() ? 1 : 0;
}

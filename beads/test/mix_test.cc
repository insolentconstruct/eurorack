// Copyright 2026 OpenAI.
//
// Author: OpenAI (support@openai.com)
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
// Mixing and output signal path tests.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <xmmintrin.h>

#define private public
#include "beads/dsp/processor.h"
#undef private
#include "beads/test/test_checker.h"

using namespace beads;

static Processor processor;
static uint8_t banks[3][262144];
static uint16_t reverb_buffer[32768];
static Parameters parameters;
static ModulatedParameter density;
static Processor::MixParameters mix;
static FloatFrame input[12];
static FloatFrame output[12];
static bool freeze[12];

void Init(bool mono = false) {
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  uint8_t* buffers[3] = { banks[0], banks[1], banks[2] };
  processor.Init(buffers, reverb_buffer);
  processor.Configure(Processor::MEDIA_DIGITAL, mono);
  memset(&parameters, 0, sizeof(parameters));
  memset(&density, 0, sizeof(density));
  memset(&mix, 0, sizeof(mix));
  memset(input, 0, sizeof(input));
  memset(freeze, 0, sizeof(freeze));
  parameters.time.base = 0.1f;
  parameters.size.base = 0.5f;
  parameters.shape.base = 0.5f;
  density.base = 0.9f;
  stmlib::Random::Seed(1);
}

void Process() {
  processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, density,
      parameters, mix, input, output, 12);
}

void FillRecording(bool mono) {
  FloatFrame sample;
  sample.l = 0.1f;
  sample.r = -0.05f;
  if (mono) {
    for (int i = 0; i < 384004; ++i) {
      processor.processor_digital_.mono_delay_line_.Store(i, sample);
    }
  } else {
    for (int i = 0; i < 192004; ++i) {
      processor.processor_digital_.stereo_delay_line_.Store(i, sample);
    }
  }
  std::fill(freeze, freeze + 12, true);
}

void TestFeedbackRecording() {
  test_checker.Start("TestFeedbackRecording");
  float recorded[2];
  for (int pass = 0; pass < 2; ++pass) {
    Init();
    FillRecording(false);
    mix.dry_wet = 1.0f;
    for (int i = 0; i < 400; ++i) {
      Process();
    }
    const FloatFrame previous =
        processor.processor_digital_.previous_sample_;
    CHECK(fabsf(previous.l) > 0.00001f);
    mix.feedback = pass ? 0.6f : 0.0f;
    processor.processor_digital_.feedback_ = mix.feedback;
    // Remove freeze's input crossfade before checking the next write.
    processor.processor_digital_.stereo_delay_line_.tail_write_ptr_ = 0;
    std::fill(freeze, freeze + 12, false);
    const int position = *processor.mutable_delay_write_ptr();
    Process();
    recorded[pass] =
        processor.processor_digital_.stereo_delay_line_.Load(position).l;
    const float expected = previous.l * mix.feedback * mix.feedback * 3.0f;
    // One 16-bit quantization step, plus float roundoff.
    CHECK(fabsf(recorded[pass] - expected) < 0.00004f);
  }
  CHECK(fabsf(recorded[1]) > fabsf(recorded[0]) + 0.00001f);
}

void TestDryWetEndpoints() {
  test_checker.Start("TestDryWetEndpoints");
  FloatFrame result[3][1200];
  for (int pass = 0; pass < 3; ++pass) {
    Init();
    FillRecording(false);
    mix.dry_wet = float(pass) * 0.5f;
    parameters.size.base = 0.8f;
    density.base = 0.1f;
    for (int i = 0; i < 12; ++i) {
      input[i].l = 0.08f;
      input[i].r = 0.04f;
    }
    for (int block = 0; block < 500; ++block) {
      Process();
      if (block >= 400) {
        std::copy(output, output + 12, result[pass] + (block - 400) * 12);
      }
    }
  }
  float dry_error = 0.0f;
  float mix_error = 0.0f;
  double wet_energy = 0.0;
  double wet_left = 0.0;
  double wet_right = 0.0;
  for (int i = 0; i < 1200; ++i) {
    dry_error = std::max(dry_error, fabsf(result[0][i].l - 0.06f));
    dry_error = std::max(dry_error, fabsf(result[0][i].r - 0.03f));
    mix_error = std::max(mix_error, fabsf(result[1][i].l - 0.75f *
        (result[0][i].l + result[2][i].l)));
    mix_error = std::max(mix_error, fabsf(result[1][i].r - 0.75f *
        (result[0][i].r + result[2][i].r)));
    wet_left += result[2][i].l;
    wet_right += result[2][i].r;
    wet_energy += result[2][i].l * result[2][i].l +
        result[2][i].r * result[2][i].r;
  }
  // Digital dry gain is 0.75; float roundoff is below 1e-5.
  CHECK(dry_error < 0.00001f);
  CHECK(mix_error < 0.00001f);
  CHECK(wet_energy > 0.01);
  CHECK(wet_left > 0.0);
  CHECK(wet_right < 0.0);
}

void TestMonoRecordingStereoOutput() {
  test_checker.Start("TestMonoRecordingStereoOutput");
  Init(true);
  FillRecording(true);
  mix.dry_wet = 1.0f;
  double energy = 0.0;
  double difference = 0.0;
  for (int block = 0; block < 1200; ++block) {
    Process();
    for (int i = 0; i < 12; ++i) {
      energy += output[i].l * output[i].l + output[i].r * output[i].r;
      const float delta = output[i].l - output[i].r;
      difference += delta * delta;
    }
  }
  CHECK(energy > 0.1);
  // Require stereo spread above 16-bit quantization noise.
  CHECK(difference > energy * 0.001);
}

// One input impulse reaches reverb through either crossfade endpoint.
double ReverbTail(bool wet, float amount) {
  Init();
  mix.dry_wet = wet ? 1.0f : 0.0f;
  mix.reverb_amount = amount;
  density.base = 0.5f;
  if (wet) {
    parameters.size.base = 1.0f;
    parameters.time.base = 0.0f;
    parameters.shape.base = 1.0f;
    density.base = 0.12f;
    processor.delay_mode_transition_ = 1.5f;
  }
  const int warmup = wet ? 1000 : 1;
  for (int block = 0; block < warmup; ++block) {
    Process();
  }
  double energy = 0.0;
  for (int block = 0; block < 2500; ++block) {
    for (int i = 0; i < 12; ++i) {
      input[i].l = block == 0 && i == 0 ? 0.6f : 0.0f;
      input[i].r = input[i].l;
    }
    Process();
    if (block >= 1000) {
      for (int i = 0; i < 12; ++i) {
        energy += output[i].l * output[i].l + output[i].r * output[i].r;
      }
    }
  }
  return energy;
}

void TestReverbAfterDryWet() {
  test_checker.Start("TestReverbAfterDryWet");
  for (int wet = 0; wet < 2; ++wet) {
    const double off = ReverbTail(wet, 0.0f);
    const double on = ReverbTail(wet, 0.85f);
    // Tail window starts 250 ms after the impulse and first delay tap.
    CHECK(off < 0.000001);
    CHECK(on > 0.0001);
  }
}

void TestReverbSendOff() {
  test_checker.Start("TestReverbSendOff");
  Init();
  mix.reverb_amount = 0.9f;
  for (int block = 0; block < 1200; ++block) {
    input[0].l = block < 400 ? 0.2f : 0.0f;
    Process();
  }
  mix.reverb_amount = 0.0f;
  double early = 0.0;
  double late = 0.0;
  for (int block = 0; block < 4000; ++block) {
    Process();
    for (int i = 0; i < 12; ++i) {
      const double power = output[i].l * output[i].l +
          output[i].r * output[i].r;
      if (block > 0 && block < 1000) {
        early += power;
      }
      if (block >= 3000) {
        late += power;
      }
    }
  }
  CHECK(early > 0.00001);
  CHECK(late < early * 0.01);
  CHECK(processor.reverb_.send_ == 0.0f);
  Init();
  for (int block = 0; block < 400; ++block) {
    input[0].l = 0.2f;
    Process();
  }
  input[0].l = 0.0f;
  double tail = 0.0;
  for (int block = 0; block < 1000; ++block) {
    Process();
    for (int i = 0; i < 12; ++i) {
      tail += output[i].l * output[i].l + output[i].r * output[i].r;
    }
  }
  CHECK(tail == 0.0);
}

template<typename T>
float FeedbackTransfer(T* granular, float previous) {
  granular->feedback_ = 1.0f;
  granular->previous_sample_.l = previous;
  granular->previous_sample_.r = previous;
  FloatFrame zero;
  zero.l = zero.r = 0.0f;
  FloatFrame out;
  bool frozen = false;
  TriggerGenerator::Output trigger;
  memset(&trigger, 0, sizeof(trigger));
  const int position = *granular->mutable_delay_write_ptr(false);
  granular->ProcessInternal(&trigger, NULL, &frozen, parameters,
      1.0f, false, &zero, &out, 1);
  return granular->stereo_delay_line_.Load(position).l;
}

void TestFeedbackQualityCurves() {
  test_checker.Start("TestFeedbackQualityCurves");
  Init();
  const float digital = FeedbackTransfer(&processor.processor_digital_, 0.5f);
  Init();
  const float vintage = FeedbackTransfer(
      &processor.processor_vintage_digital_, 0.5f);
  Init();
  const float clean = FeedbackTransfer(
      &processor.processor_clean_tape_, 0.5f);
  Init();
  const float dirty = FeedbackTransfer(
      &processor.processor_dirty_tape_, 0.5f);
  CHECK(digital == 32767.0f / 32768.0f);
  // SoftLimit(1.5) = 13/14; allow one 12-bit or mu-law step.
  CHECK(fabsf(vintage - 13.0f / 14.0f) < 1.0f / 2048.0f);
  CHECK(clean == vintage);
  CHECK(fabsf(dirty - 13.0f / 14.0f) < 0.02f);
  Init();
  const float digital_low = FeedbackTransfer(
      &processor.processor_digital_, 0.1f);
  Init();
  const float vintage_low = FeedbackTransfer(
      &processor.processor_vintage_digital_, 0.1f);
  Init();
  const float dirty_low = FeedbackTransfer(
      &processor.processor_dirty_tape_, 0.1f);
  CHECK(fabsf(digital_low - 0.3f) < 0.00004f);
  CHECK(vintage_low < digital_low - 0.001f);
  CHECK(dirty_low < vintage_low - 0.001f);
}

static float echoes[2][12000];

int EchoOnset(float time, float feedback, float* samples) {
  Init();
  parameters.size.base = 1.0f;
  parameters.time.base = time;
  parameters.shape.base = 1.0f;
  density.base = 0.05f;
  mix.dry_wet = 1.0f;
  mix.feedback = feedback;
  // State reached after holding SIZE fully clockwise for 1.5 seconds.
  processor.delay_mode_transition_ = 1.5f;
  for (int block = 0; block < 1500; ++block) {
    Process();
  }
  int onset = -1;
  for (int block = 0; block < 1000; ++block) {
    for (int i = 0; i < 12; ++i) {
      input[i].l = block == 0 ? 0.3f : 0.0f;
      input[i].r = input[i].l;
    }
    Process();
    for (int i = 0; i < 12; ++i) {
      const int index = block * 12 + i;
      samples[index] = output[i].l;
      if (onset < 0 && fabsf(samples[index]) > 0.001f) {
        onset = index;
      }
    }
  }
  return onset;
}

void TestFeedbackDelayReturn() {
  test_checker.Start("TestFeedbackDelayReturn");
  const int short_onset = EchoOnset(0.0f, 0.7f, echoes[0]);
  const int long_onset = EchoOnset(0.25f, 0.7f, echoes[1]);
  CHECK(short_onset > 100);
  // TIME selects multipliers 1 and 5; allow controller smoothing.
  CHECK(long_onset > short_onset * 3);
  const int reference_onset = EchoOnset(0.25f, 0.0f, echoes[0]);
  CHECK(reference_onset == long_onset);
  double return_energy = 0.0;
  double reference_energy = 0.0;
  // Exclude the 12-sample excitation and interpolation shoulders.
  for (int i = long_onset + 100; i < 12000; ++i) {
    return_energy += echoes[1][i] * echoes[1][i];
    reference_energy += echoes[0][i] * echoes[0][i];
  }
  CHECK(return_energy > 0.001);
  CHECK(reference_energy < return_energy * 0.01);
}

int main() {
  // Match the firmware FPU setup in drivers/system.cc.
  _mm_setcsr(_mm_getcsr() | 0x8040);
  TestFeedbackRecording();
  TestDryWetEndpoints();
  TestMonoRecordingStereoOutput();
  TestReverbAfterDryWet();
  TestReverbSendOff();
  TestFeedbackQualityCurves();
  TestFeedbackDelayReturn();
  return test_checker.failures() ? 1 : 0;
}

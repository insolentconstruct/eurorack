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
// Mixing and output signal path tests.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif  // defined(__SSE__)

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

void Init(bool mono) {
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
    Init(false);
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
    Init(false);
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
  Init(false);
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
  Init(false);
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
  Init(false);
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
  Init(false);
  const float digital = FeedbackTransfer(&processor.processor_digital_, 0.5f);
  Init(false);
  const float vintage = FeedbackTransfer(
      &processor.processor_vintage_digital_, 0.5f);
  Init(false);
  const float clean = FeedbackTransfer(
      &processor.processor_clean_tape_, 0.5f);
  Init(false);
  const float dirty = FeedbackTransfer(
      &processor.processor_dirty_tape_, 0.5f);
  CHECK(digital == 32767.0f / 32768.0f);
  // SoftLimit(1.5) = 13/14; allow one 12-bit or mu-law step.
  CHECK(fabsf(vintage - 13.0f / 14.0f) < 1.0f / 2048.0f);
  CHECK(clean == vintage);
  CHECK(fabsf(dirty - 13.0f / 14.0f) < 0.02f);
  Init(false);
  const float digital_low = FeedbackTransfer(
      &processor.processor_digital_, 0.1f);
  Init(false);
  const float vintage_low = FeedbackTransfer(
      &processor.processor_vintage_digital_, 0.1f);
  Init(false);
  const float dirty_low = FeedbackTransfer(
      &processor.processor_dirty_tape_, 0.1f);
  CHECK(fabsf(digital_low - 0.3f) < 0.00004f);
  CHECK(vintage_low < digital_low - 0.001f);
  CHECK(dirty_low < vintage_low - 0.001f);
}

static float echoes[2][12000];

int EchoOnset(float time, float feedback, float* samples) {
  Init(false);
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

template<Resolution resolution, int max_delay, int ratio>
void FillSendRecording(GranularProcessor<resolution, max_delay, ratio>* p,
    bool mono) {
  FloatFrame sample;
  sample.l = 0.1f;
  sample.r = -0.05f;
  for (int i = 0; i < max_delay * (mono ? 2 : 1) + 4; ++i) {
    if (mono) {
      p->mono_delay_line_.Store(i, sample);
    } else {
      p->stereo_delay_line_.Store(i, sample);
    }
  }
}

void InitSendRecording(Processor::Media media, bool mono) {
  Init(mono);
  processor.Configure(media, mono);
  switch (media) {
    case Processor::MEDIA_DIGITAL:
      FillSendRecording(&processor.processor_digital_, mono);
      break;
    case Processor::MEDIA_VINTAGE_DIGITAL:
      FillSendRecording(&processor.processor_vintage_digital_, mono);
      break;
    case Processor::MEDIA_CLEAN_TAPE:
      FillSendRecording(&processor.processor_clean_tape_, mono);
      break;
    case Processor::MEDIA_DIRTY_TAPE:
      FillSendRecording(&processor.processor_dirty_tape_, mono);
      break;
  }
  std::fill(freeze, freeze + 12, true);
  processor.set_send_mix(true);
  processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
}

void TestSendMixQualities() {
  test_checker.Start("TestSendMixQualities");
  FloatFrame tail[3][1200];
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      for (int pass = 0; pass < 3; ++pass) {
        InitSendRecording(Processor::Media(media), mono);
        mix.dry_wet = pass == 2 ? 1.0f : 0.5f;
        parameters.size.base = 0.8f;
        density.base = 0.5f;
        stmlib::GateFlags seed[12];
        std::fill(seed, seed + 12, stmlib::GATE_FLAG_LOW);
        seed[0] = stmlib::GateFlags(
            stmlib::GATE_FLAG_RISING | stmlib::GATE_FLAG_HIGH);
        processor.Process(SEED_BUTTON_STATE_OFF, seed, freeze, density,
            parameters, mix, input, output, 12);
        for (int block = 0; block < 100; ++block) {
          if (pass == 1) {
            mix.dry_wet = 0.0f;
          }
          processor.Process(SEED_BUTTON_STATE_OFF, NULL, freeze, density,
              parameters, mix, input, output, 12);
          std::copy(output, output + 12, tail[pass] + block * 12);
        }
      }
      CHECK(memcmp(tail[0], tail[1], sizeof(tail[0])) == 0);
      double energy = 0.0;
      float error = 0.0f;
      for (int i = 0; i < 1200; ++i) {
        energy += tail[1][i].l * tail[1][i].l +
            tail[1][i].r * tail[1][i].r;
        error = std::max(error, fabsf(tail[1][i].l * 2.0f - tail[2][i].l));
        error = std::max(error, fabsf(tail[1][i].r * 2.0f - tail[2][i].r));
      }
      CHECK(energy > 0.001);
      CHECK(error < 1e-6f);
    }
  }
}

void TestSendMixOtherProcessors() {
  test_checker.Start("TestSendMixOtherProcessors");
  static FloatFrame reference[1200 * 12];
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      for (int spectral = 0; spectral < 2; ++spectral) {
        for (int grains_reverb = 0; grains_reverb < 2; ++grains_reverb) {
          for (int send = 0; send < 2; ++send) {
            memset(banks, 0, sizeof(banks));
            memset(reverb_buffer, 0, sizeof(reverb_buffer));
            Init(mono);
            processor.set_spectral(spectral);
            processor.Configure(Processor::Media(media), mono);
            processor.set_send_mix(send);
            processor.set_reverb_on_grains(grains_reverb);
            if (!spectral) {
              processor.delay_mode_transition_ = 1.5f;
              parameters.size.base = 1.0f;
            }
            mix.feedback = 0.3f;
            mix.reverb_amount = 0.6f;
            const size_t size = size_t(processor.sample_rate()) / 4000;
            bool identical = true;
            double energy = 0.0;
            for (int block = 0; block < 1200; ++block) {
              mix.dry_wet = block < 400 ? 0.0f
                  : (block < 800 ? 0.4f : 1.0f);
              for (size_t i = 0; i < size; ++i) {
                const float t = float(block * size + i);
                input[i].l = 0.12f * sinf(t * 0.031f);
                input[i].r = 0.08f * sinf(t * 0.017f);
              }
              processor.Buffer();
              processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze,
                  density, parameters, mix, input, output, size);
              for (size_t i = 0; i < size; ++i) {
                if (send) {
                  const FloatFrame& expected = reference[block * 12 + i];
                  identical &= output[i].l == expected.l &&
                      output[i].r == expected.r;
                } else {
                  reference[block * 12 + i] = output[i];
                }
                energy += output[i].l * output[i].l +
                    output[i].r * output[i].r;
              }
            }
            CHECK(identical);
            CHECK(energy > 0.01);
          }
        }
      }
    }
  }
}

void TestSendMix() {
  test_checker.Start("TestSendMix");
  // One sustained grain: its complete tail is independent of later send moves.
  FloatFrame tail[3][1200];
  for (int pass = 0; pass < 3; ++pass) {
    Init(false);
    FillRecording(false);
    processor.set_send_mix(true);
    processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    mix.dry_wet = pass == 2 ? 1.0f : 0.5f;
    parameters.size.base = 0.8f;
    density.base = 0.5f;
    stmlib::GateFlags seed[12];
    std::fill(seed, seed + 12, stmlib::GATE_FLAG_LOW);
    seed[0] = stmlib::GateFlags(
        stmlib::GATE_FLAG_RISING | stmlib::GATE_FLAG_HIGH);
    processor.Process(SEED_BUTTON_STATE_OFF, seed, freeze, density,
        parameters, mix, input, output, 12);
    for (int block = 0; block < 100; ++block) {
      if (pass == 1) {
        mix.dry_wet = 0.0f;
      }
      processor.Process(SEED_BUTTON_STATE_OFF, NULL, freeze, density,
          parameters, mix, input, output, 12);
      std::copy(output, output + 12, tail[pass] + block * 12);
    }
  }
  CHECK(memcmp(tail[0], tail[1], sizeof(tail[0])) == 0);
  double energy = 0.0;
  for (int i = 0; i < 1200; ++i) {
    energy += tail[1][i].l * tail[1][i].l;
    CHECK(fabsf(tail[1][i].l * 2.0f - tail[2][i].l) < 1e-6f);
    CHECK(fabsf(tail[1][i].r * 2.0f - tail[2][i].r) < 1e-6f);
  }
  CHECK(energy > 0.001);

  // At zero send the recorder advances, dry stays unity, and no grains start.
  Init(false);
  processor.set_send_mix(true);
  processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  mix.reverb_amount = 0.8f;
  const int position = *processor.mutable_delay_write_ptr();
  for (int i = 0; i < 12; ++i) {
    input[i].l = 0.1f;
    input[i].r = -0.05f;
  }
  for (int block = 0; block < 100; ++block) {
    Process();
  }
  const int written = *processor.mutable_delay_write_ptr();
  CHECK(written != position);
  const FloatFrame recorded =
      processor.processor_digital_.stereo_delay_line_.Load(written + 1);
  CHECK(fabsf(recorded.l - input[11].l) < 0.00004f);
  CHECK(fabsf(recorded.r - input[11].r) < 0.00004f);
  CHECK(processor.processor_digital_.granular_player_.CountActiveGrains() == 0);
  CHECK(fabsf(output[11].l - 0.075f) < 0.00001f);
  CHECK(fabsf(output[11].r + 0.0375f) < 0.00001f);
  CHECK(processor.wet_ == 1.5f);
  // Raising the send finds the recording, without another recording pass.
  std::fill(input, input + 12, FloatFrame());
  std::fill(freeze, freeze + 12, true);
  mix.dry_wet = 1.0f;
  for (int block = 0; block < 100; ++block) {
    Process();
  }
  CHECK(processor.processor_digital_.granular_player_.CountActiveGrains() > 0);
  // Dry keeps its fully dry level at every send setting, with no grain clock.
  for (int level = 0; level <= 4; ++level) {
    Init(false);
    processor.set_send_mix(true);
    processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    density.base = 0.5f;
    mix.dry_wet = float(level) * 0.25f;
    mix.reverb_amount = 0.8f;
    for (int i = 0; i < 12; ++i) {
      input[i].l = 0.1f;
      input[i].r = -0.05f;
    }
    for (int block = 0; block < 8; ++block) {
      Process();
    }
    CHECK(fabsf(output[11].l - 0.075f) < 0.00001f);
    CHECK(fabsf(output[11].r + 0.0375f) < 0.00001f);
  }

}

// Gains reached after a settled block: stock, or the gentle wet law.
void MixGains(bool gentle, float dry_wet, float* dry, float* wet) {
  Init(false);
  processor.set_gentle_mix(gentle);
  mix.dry_wet = dry_wet;
  for (int i = 0; i < 4; ++i) {
    Process();
  }
  *dry = processor.dry_;
  *wet = processor.wet_;
}

// Rendered grain level with no input, the recording frozen.
double WetEnergy(bool gentle, float dry_wet) {
  Init(false);
  FillRecording(false);
  processor.set_gentle_mix(gentle);
  processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  mix.dry_wet = dry_wet;
  parameters.size.base = 0.8f;
  density.base = 0.1f;
  double energy = 0.0;
  for (int block = 0; block < 500; ++block) {
    Process();
    if (block >= 400) {
      for (int i = 0; i < 12; ++i) {
        energy += output[i].l * output[i].l + output[i].r * output[i].r;
      }
    }
  }
  return energy;
}

void TestMixLaw() {
  test_checker.Start("TestMixLaw");
  const float settings[] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.5f, 0.75f, 1.0f };
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
    const float x = settings[i];
    float dry[2];
    float wet[2];
    MixGains(false, x, &dry[0], &wet[0]);
    MixGains(true, x, &dry[1], &wet[1]);
    CHECK(fabsf(dry[0] - (1.0f - x) * (1.0f + x) * 0.75f) < 1e-6f);
    CHECK(dry[1] == dry[0]);
    CHECK(fabsf(wet[0] - x * (2.0f - x) * 1.5f) < 1e-6f);
    CHECK(fabsf(wet[1] - x * 1.5f) < 1e-6f);
  }
  // At 9 o'clock (20%) the wet is 7.6 dB under the dry, not 2.5 dB.
  float dry;
  float wet;
  MixGains(true, 0.2f, &dry, &wet);
  CHECK(fabsf(20.0f * log10f(wet / dry) + 7.604f) < 0.01f);
  MixGains(false, 0.2f, &dry, &wet);
  CHECK(fabsf(20.0f * log10f(wet / dry) + 2.499f) < 0.01f);
  // The rendered grains follow the law; both ends match stock.
  const double full = WetEnergy(false, 1.0f);
  CHECK(full > 0.01);
  CHECK(WetEnergy(true, 1.0f) == full);
  CHECK(WetEnergy(true, 0.0f) == 0.0);
  const double stock = WetEnergy(false, 0.2f) / full;
  const double gentle = WetEnergy(true, 0.2f) / full;
  CHECK(fabs(stock - 0.36 * 0.36) < 1e-4);
  CHECK(fabs(gentle - 0.2 * 0.2) < 1e-4);
}

// Reverb tail energy after an input burst; grains silent unless recorded.
// With a recording, DRY/WET goes to zero, or new grains stop.
double GrainsReverbTail(bool grains, float dry_wet, bool record, bool cut) {
  Init(false);
  if (record) {
    FillRecording(false);
  }
  processor.set_reverb_on_grains(grains);
  processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  mix.dry_wet = dry_wet;
  mix.reverb_amount = 0.85f;
  for (int block = 0; block < 400; ++block) {
    for (int i = 0; i < 12; ++i) {
      input[i].l = block < 100 ? 0.3f : 0.0f;
      input[i].r = input[i].l;
    }
    Process();
  }
  if (record && cut) {
    mix.dry_wet = 0.0f;
  } else if (record) {
    density.base = 0.5f;
  }
  // Only the reverb's tail remains once the grains have ended.
  double tail = 0.0;
  for (int block = 0; block < 6000; ++block) {
    Process();
    if (block >= 4000) {
      for (int i = 0; i < 12; ++i) {
        tail += output[i].l * output[i].l + output[i].r * output[i].r;
      }
    }
  }
  return tail;
}

void TestReverbOnGrains() {
  test_checker.Start("TestReverbOnGrains");
  // Fully dry: stock sends the dry to the reverb, Grains has no reverb.
  CHECK(GrainsReverbTail(false, 0.0f, false, true) > 0.0001);
  CHECK(GrainsReverbTail(true, 0.0f, false, true) == 0.0);
  // Grains reach the reverb; DRY/WET then sets the tail too.
  CHECK(GrainsReverbTail(false, 1.0f, true, true) > 0.0001);
  CHECK(GrainsReverbTail(true, 1.0f, true, false) > 0.0001);
  CHECK(GrainsReverbTail(true, 1.0f, true, true) == 0.0);
  CHECK(GrainsReverbTail(false, 1.0f, true, false) > 0.0001);
  // The dry is neither sent nor ducked: with no grains it passes as is.
  for (int grains = 0; grains < 2; ++grains) {
    Init(false);
    density.base = 0.5f;
    processor.set_reverb_on_grains(grains);
    processor.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    mix.dry_wet = 0.3f;
    mix.reverb_amount = 0.9f;
    float error = 0.0f;
    for (int block = 0; block < 400; ++block) {
      for (int i = 0; i < 12; ++i) {
        input[i].l = 0.2f;
        input[i].r = -0.1f;
      }
      Process();
      if (block >= 300) {
        error = std::max(error,
            fabsf(output[0].l - 0.2f * 0.91f * 0.75f));
      }
    }
    CHECK(grains ? error < 1e-6f : error > 0.01f);
  }
  // Delay and spectral take the same order: fully dry has no reverb.
  for (int spectral = 0; spectral < 2; ++spectral) {
    for (int grains = 0; grains < 2; ++grains) {
      Init(false);
      if (spectral) {
        processor.set_spectral(true);
        processor.Configure(Processor::MEDIA_DIGITAL, false);
      } else {
        processor.delay_mode_transition_ = 1.5f;
        parameters.size.base = 1.0f;
      }
      processor.set_reverb_on_grains(grains);
      mix.reverb_amount = 0.85f;
      double tail = 0.0;
      for (int block = 0; block < 1500; ++block) {
        for (int i = 0; i < 12; ++i) {
          input[i].l = block < 100 ? 0.3f : 0.0f;
          input[i].r = input[i].l;
        }
        processor.Buffer();
        Process();
        if (block >= 600) {
          for (int i = 0; i < 12; ++i) {
            tail += output[i].l * output[i].l + output[i].r * output[i].r;
          }
        }
      }
      CHECK(grains ? tail == 0.0 : tail > 0.0001);
    }
  }
}

static uint8_t stock_banks[3][262144];
static FloatFrame stock_output[2000 * 12];

// The options block matches the stock block when no option is on.
void TestOptionsBlockMatchesStock() {
  test_checker.Start("TestOptionsBlockMatchesStock");
  for (int scenario = 0; scenario < 12; ++scenario) {
    const Processor::Media media = Processor::Media(scenario % 4);
    for (int path = 0; path < 2; ++path) {
      memset(banks, 0, sizeof(banks));
      memset(reverb_buffer, 0, sizeof(reverb_buffer));
      Init(scenario == 4 || scenario == 7);
      processor.set_spectral(scenario >= 8);
      if (scenario >= 4) {
        processor.SetCloudsStyle(true);
        processor.set_diffusion(0.5f);
        processor.set_evolving_feedback(true);
      }
      processor.Configure(media, scenario == 4 || scenario == 7);
      mix.dry_wet = 0.6f;
      mix.reverb_amount = 0.7f;
      mix.feedback = 0.4f;
      mix.output_gate = scenario == 5;
      const size_t size = size_t(processor.sample_rate()) / 4000;
      int differences = 0;
      for (int block = 0; block < 2000; ++block) {
        for (size_t i = 0; i < size; ++i) {
          const float t = float(block * size + i);
          input[i].l = 0.3f * sinf(t * 0.031f) * (block % 300 < 150);
          input[i].r = 0.2f * sinf(t * 0.017f);
          freeze[i] = block >= 1200 && block < 1400;
        }
        // Delay mode from block 500, fading back towards granular.
        if (block == 500 && scenario < 8) {
          processor.delay_mode_transition_ = 0.8f;
        }
        parameters.size.base = block >= 500 && block < 900 ? 1.0f : 0.5f;
        processor.Buffer();
        if (path) {
          processor.ProcessWithOptions(SEED_BUTTON_STATE_LOCKED, NULL, freeze,
              density, parameters, mix, input, output, size);
        } else {
          processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, density,
              parameters, mix, input, output, size);
        }
        FloatFrame* reference = stock_output + block * 12;
        for (size_t i = 0; i < size; ++i) {
          if (path) {
            differences += output[i].l != reference[i].l ||
                output[i].r != reference[i].r;
          } else {
            reference[i] = output[i];
          }
        }
      }
      if (!path) {
        memcpy(stock_banks, banks, sizeof(banks));
      } else {
        CHECK(differences == 0);
        CHECK(!memcmp(stock_banks, banks, sizeof(banks)));
      }
    }
  }
}

void TestOutputDivision() {
  test_checker.Start("TestOutputDivision");
  FloatFrame reference[120];
  for (int division = 1; division <= 16; division *= 2) {
    Init(false);
    density.base = 0.5f;
    mix.output_gate = true;
    mix.dry_wet = 0.5f;
    processor.set_output_division(division);
    CHECK(bool(processor.options_ & Processor::OPTION_FLAG_OUTPUT_DIVISION)
        == (division > 1));
    for (int block = 0; block < 10; ++block) {
      stmlib::GateFlags seed[12];
      for (int i = 0; i < 12; ++i) {
        const int position = block * 12 + i;
        const int phase = position % 8;
        seed[i] = phase == 0 ? stmlib::GATE_FLAG_RISING
            : stmlib::GATE_FLAG_LOW;
        if (phase < 3) {
          seed[i] = stmlib::GateFlags(seed[i] | stmlib::GATE_FLAG_HIGH);
        }
        input[i].l = 0.1f * sinf(float(position) * 0.13f);
        input[i].r = -input[i].l;
      }
      processor.Process(SEED_BUTTON_STATE_OFF, seed, freeze, density,
          parameters, mix, input, output, 12);
      for (int i = 0; i < 12; ++i) {
        const int position = block * 12 + i;
        const bool expected = position % 8 < 3
            && (position / 8) % division == 0;
        CHECK(output[i].r == (expected ? -0.8f : 0.0f));
        if (division == 1) {
          reference[position] = output[i];
        } else {
          CHECK(output[i].l == reference[position].l);
        }
      }
    }
    processor.set_output_division(division);
    if (division > 1) {
      CHECK(processor.output_pulse_count_ == 15 % division);
    }
    processor.set_output_division(1);
    CHECK(!(processor.options_ & Processor::OPTION_FLAG_OUTPUT_DIVISION));
  }
}

int main() {
  // Match the firmware FPU setup in drivers/system.cc.
#if defined(__SSE__)
  _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
  uint64_t fpcr;
  __asm__ volatile("mrs %0, fpcr" : "=r" (fpcr));
  fpcr |= uint64_t(1) << 24;
  __asm__ volatile("msr fpcr, %0" : : "r" (fpcr));
#endif  // defined(__SSE__)
  TestSendMixQualities();
  TestSendMixOtherProcessors();
  TestSendMix();
  TestOutputDivision();
  TestFeedbackRecording();
  TestDryWetEndpoints();
  TestMonoRecordingStereoOutput();
  TestReverbAfterDryWet();
  TestReverbSendOff();
  TestFeedbackQualityCurves();
  TestFeedbackDelayReturn();
  TestMixLaw();
  TestReverbOnGrains();
  TestOptionsBlockMatchesStock();
  return test_checker.failures() ? 1 : 0;
}

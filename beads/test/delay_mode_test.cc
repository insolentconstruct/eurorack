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
#include "beads/test/test_runner.h"

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
  float amplitude;
  SeedButtonState seed;
  const stmlib::GateFlags* seed_input;
  bool frozen;
  bool latch_fade;
  // A Hann-windowed tone burst when the length is positive.
  int burst_start;
  int burst_length;

  Rig() : sample(0), frequency(750.0f), amplitude(0.05f),
      seed(SEED_BUTTON_STATE_LOCKED), seed_input(NULL), frozen(false),
      latch_fade(true), burst_start(0), burst_length(0) {
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

  void Block(std::vector<float>* capture) {
    const int size = int(processor.sample_rate()) / 4000;
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12];
    std::fill(&freeze[0], &freeze[12], frozen);
    for (int i = 0; i < size; ++i) {
      float x = amplitude * sin(
          6.283185307179586 * frequency *
          (sample % int(processor.sample_rate())) / processor.sample_rate());
      if (burst_length > 0) {
        const int n = sample - burst_start;
        x = n >= 0 && n < burst_length
            ? x * (0.5f - 0.5f * cosf(6.2831853f * n / burst_length))
            : 0.0f;
      }
      ++sample;
      in[i].l = x;
      in[i].r = x;
    }
    processor.Process(seed, seed_input, freeze, density,
        parameters, mix, in, out, size);
    if (capture) {
      for (int i = 0; i < size; ++i) {
        capture->push_back(out[i].l);
      }
    }
  }

  void Run(int blocks) {
    while (blocks--) {
      Block(NULL);
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
      r.Block(NULL);
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

void CheckQuantizerDelayBypass(int quality) {
  for (int mono = 0; mono < 2; ++mono) {
    Rig r;
    r.processor.Configure(Processor::Media(quality), mono);
    r.parameters.pitch.base = 3.4f;
    r.parameters.pitch.attenuverter = 0.8f;
    stmlib::Random::Seed(123);
    r.Run(16000);
    CHECK(r.processor.delay_mode_transition_ > 1.49f);
    // Both comparisons start from exactly the same warmed processor. Restore
    // into the same Rig so its internal buffer pointers remain valid.
    std::vector<uint8_t> saved(sizeof(r));
    memcpy(&saved[0], &r, sizeof(r));
    std::vector<float> expected;
    std::vector<float> actual;
    stmlib::Random::Seed(123);
    for (int i = 0; i < 1200; ++i) {
      r.Block(&expected);
    }
    memcpy(static_cast<void*>(&r), &saved[0], sizeof(r));
    r.processor.set_pitch_scale(PITCH_SCALE_PELOG);
    stmlib::Random::Seed(123);
    for (int i = 0; i < 1200; ++i) {
      r.Block(&actual);
    }
    CHECK(expected == actual);
  }
}

void TestQuantizerDelayBypassDigital() {
  test_checker.Start("TestQuantizerDelayBypassDigital");
  CheckQuantizerDelayBypass(0);
  CheckQuantizerDelayBypass(1);
}

void TestQuantizerDelayBypassTape() {
  test_checker.Start("TestQuantizerDelayBypassTape");
  CheckQuantizerDelayBypass(2);
  CheckQuantizerDelayBypass(3);
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
      r.Run(64);
      CHECK(r.processor.delay_mode_transition_ < -1.49f);
      // Full fade timing is covered by TestDelayModeFade. Exercise both
      // directions at the mode boundary for each quality/channel pair.
      r.processor.delay_mode_transition_ = -0.001f;
      r.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
      r.Run(64);
      CHECK(r.processor.delay_mode_transition_ > 0.0f);
      r.processor.delay_mode_transition_ = 0.001f;
      r.processor.set_delay_mode(DELAY_MODE_NEVER);
      r.Run(64);
      CHECK(r.processor.delay_mode_transition_ < 0.0f);
    }
  }
  // Q is Butterworth at both ends, rather than the stock resonant boost.
  for (int end = 0; end < 2; ++end) {
    Rig r;
    r.processor.Configure(Processor::MEDIA_DIGITAL, false);
    r.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
    r.parameters.size.attenuverter = end ? 1.0f : -1.0f;
    r.processor.delay_mode_transition_ = 1.5f;
    r.Run(3000);
    CHECK(fabsf(r.processor.processor_digital_.multitap_delay_.filter_[0].r()
        - 1.41421356f) < 0.000001f);
  }
  // The neutral target bypasses the filter, including additive SIZE CV.
  Rig a;
  Rig b;
  a.processor.Configure(Processor::MEDIA_DIGITAL, false);
  b.processor.Configure(Processor::MEDIA_DIGITAL, false);
  b.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
  // Start in delay so SIZE randomness cannot change the warm-up grains.
  a.processor.delay_mode_transition_ = 1.5f;
  b.processor.delay_mode_transition_ = 1.5f;
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
  // MI's interpolator can settle one float step away from 0.5. Changing Q
  // must leave the neutral result within that rounding noise (-140 dBFS).
  for (size_t i = 0; i < stock.size(); ++i) {
    CHECK(fabsf(stock[i] - detent[i]) < 0.0000001f);
  }
}

void TestDelayFilterRamp() {
  test_checker.Start("TestDelayFilterRamp");
  CHECK(sizeof(DelayFilterMode) == 4);
  DelayFilterMode mode = { true, 0, 0 };
  DelayFilterQ previous = { kStockDelayQOffset, kStockDelayQScale };
  for (int direction = 0; direction < 2; ++direction) {
    mode.wide_detent = direction == 0;
    for (int block = 0; block < 3000; ++block) {
      const DelayFilterQ q = SmoothDelayFilterQ(&mode,
          mode.wide_detent ? kWideDetentQOffset : kStockDelayQOffset,
          mode.wide_detent ? 0.0f : kStockDelayQScale);
      CHECK(fabsf(q.offset - previous.offset) < 0.0015f);
      CHECK(fabsf(q.scale - previous.scale) < 0.081f);
      previous = q;
    }
    CHECK(previous.offset == (direction
      ? kStockDelayQOffset : kWideDetentQOffset));
    CHECK(mode.position == (direction ? 0 : kDelayQPositionMax));
    // Fused arithmetic can retain a few billionths at the zero endpoint.
    CHECK(fabsf(previous.scale - (direction ? kStockDelayQScale : 0.0f))
        < 1e-8f);
  }
}

float MaximumStep(const std::vector<float>& audio, size_t size) {
  float step = 0.0f;
  for (size_t i = 1; i < size; ++i) {
    step = std::max(step, fabsf(audio[i] - audio[i - 1]));
  }
  return step;
}

void TestDelayModeSwitchClick() {
  test_checker.Start("TestDelayModeSwitchClick");
  Rig r;
  r.frequency = 750.0f;
  r.parameters.size.attenuverter = 1.0f;
  r.processor.delay_mode_transition_ = 1.5f;
  r.Run(14000);
  std::vector<uint8_t> saved(sizeof(r));
  memcpy(&saved[0], &r, sizeof(r));
  std::vector<float> control;
  for (int i = 0; i < 4096; ++i) {
    r.Block(&control);
  }
  memcpy(static_cast<void*>(&r), &saved[0], sizeof(r));
  std::vector<float> switched;
  r.Block(&switched);
  r.processor.set_delay_mode(DELAY_MODE_WIDE_DETENT);
  for (int i = 0; i < 4095; ++i) {
    r.Block(&switched);
  }
  const size_t checkpoints[] = { 100, 512, 4096 };
  const size_t block_size = int(r.processor.sample_rate()) / 4000;
  CHECK(switched.size() == control.size());
  for (int i = 0; i < 3; ++i) {
    const size_t frames = checkpoints[i] * block_size;
    const float switched_step = MaximumStep(switched, frames);
    const float control_step = MaximumStep(control, frames);
    printf("%u blocks, switch/control steps: %.9g / %.9g\n",
        static_cast<uint32_t>(checkpoints[i]), switched_step, control_step);
    CHECK(switched_step <= control_step);
  }
}

const stmlib::GateFlags kSeedLow[12] = {
  stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW,
  stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW,
  stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW,
  stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW, stmlib::GATE_FLAG_LOW,
};

// Delay mode at DENSITY 0.3 with a tapped 500 ms tempo, unlatched (#183).
void PrepareTappedDelay(Rig* r, int media, int offset) {
  r->processor.Configure(Processor::Media(media), false);
  r->processor.set_seed_latch_fade(r->latch_fade);
  r->frequency = 220.0f;
  r->amplitude = 0.25f;
  r->density.base = 0.3f;
  r->seed = SEED_BUTTON_STATE_OFF;
  r->processor.delay_mode_transition_ = 1.5f;
  r->Run(4000 + offset);
  for (int tap = 0; tap < 2; ++tap) {
    r->seed = SEED_BUTTON_STATE_ON;
    r->Run(400);
    r->seed = SEED_BUTTON_STATE_OFF;
    r->Run(1600);
  }
}

// Largest step within 100 ms of a SEED change, against the steady output
// in the 500 ms before and in 500 ms starting 200 ms after it.
float SeedStepRatio(Rig* r, SeedButtonState seed,
    const stmlib::GateFlags* seed_input) {
  std::vector<float> before;
  for (int i = 0; i < 2000; ++i) {
    r->Block(&before);
  }
  r->seed = seed;
  r->seed_input = seed_input;
  std::vector<float> event;
  for (int i = 0; i < 400; ++i) {
    r->Block(&event);
  }
  r->Run(400);
  std::vector<float> after;
  for (int i = 0; i < 2000; ++i) {
    r->Block(&after);
  }
  const float reference = std::max(
      MaximumStep(before, before.size()),
      MaximumStep(after, after.size()));
  const float step = std::max(
      MaximumStep(event, event.size()), fabsf(event[0] - before.back()));
  // Silence or NaN would make the ratio pass vacuously.
  bool finite = true;
  for (size_t i = 0; i < event.size(); ++i) {
    finite = finite && fabsf(event[i]) < 10.0f;
  }
  CHECK(finite);
  CHECK(reference > 0.001f);
  return step / reference;
}

float LatchAndUnlatch(Rig* r, const stmlib::GateFlags* seed_input) {
  const float latch = SeedStepRatio(r, SEED_BUTTON_STATE_LOCKED, seed_input);
  CHECK(!r->processor.delay_controller_.test_synchronized());
  // Unlatching switches back at the press.
  const float unlatch = SeedStepRatio(r, SEED_BUTTON_STATE_ON, seed_input);
  CHECK(r->processor.delay_controller_.test_synchronized());
  return std::max(latch, unlatch);
}

void TestSeedLatchFade() {
  test_checker.Start("TestSeedLatchFade");
  // SEED unpatched: latching switches tapped to knob timing, then back.
  float worst = 0.0f;
  for (int media = 0; media < 4; ++media) {
    for (int offset = 0; offset < 300; offset += 101) {
      Rig r;
      PrepareTappedDelay(&r, media, offset);
      worst = std::max(worst, LatchAndUnlatch(&r, NULL));
    }
  }
  printf("Unpatched latch/unlatch step ratio: %.3f\n", worst);
  CHECK(worst < 1.5f);

  // FEEDBACK at noon, and FREEZE.
  Rig feedback;
  feedback.mix.feedback = 0.5f;
  PrepareTappedDelay(&feedback, Processor::MEDIA_DIGITAL, 0);
  const float feedback_ratio = LatchAndUnlatch(&feedback, NULL);
  Rig frozen;
  PrepareTappedDelay(&frozen, Processor::MEDIA_DIGITAL, 0);
  frozen.frozen = true;
  frozen.Run(2000);
  const float frozen_ratio = LatchAndUnlatch(&frozen, NULL);
  printf("Feedback, frozen step ratio: %.3f, %.3f\n",
      feedback_ratio, frozen_ratio);
  CHECK(feedback_ratio < 1.5f);
  CHECK(frozen_ratio < 1.5f);

  // The free-running latch option ignores a patched SEED input.
  Rig free_running;
  free_running.seed_input = kSeedLow;
  PrepareTappedDelay(&free_running, Processor::MEDIA_DIGITAL, 0);
  const float free_running_ratio = SeedStepRatio(
      &free_running, SEED_BUTTON_STATE_IGNORE, kSeedLow);
  CHECK(!free_running.processor.delay_controller_.test_synchronized());

  // Unplugging and replugging SEED while latched.
  Rig cable;
  cable.seed_input = kSeedLow;
  PrepareTappedDelay(&cable, Processor::MEDIA_DIGITAL, 0);
  cable.seed = SEED_BUTTON_STATE_LOCKED;
  cable.Run(2000);
  const float unplug = SeedStepRatio(&cable, SEED_BUTTON_STATE_LOCKED, NULL);
  CHECK(!cable.processor.delay_controller_.test_synchronized());
  const float plug = SeedStepRatio(
      &cable, SEED_BUTTON_STATE_LOCKED, kSeedLow);
  CHECK(cable.processor.delay_controller_.test_synchronized());
  printf("Free-running, unplug, plug step ratio: %.3f, %.3f, %.3f\n",
      free_running_ratio, unplug, plug);
  CHECK(free_running_ratio < 1.5f);
  CHECK(unplug < 1.5f);
  CHECK(plug < 1.5f);
}

void TestSeedLatchFadePolicy() {
  test_checker.Start("TestSeedLatchFadePolicy");
  // A clocked latch keeps the timing, and never touches the tap gains.
  Rig clocked;
  clocked.seed_input = kSeedLow;
  PrepareTappedDelay(&clocked, Processor::MEDIA_DIGITAL, 0);
  clocked.seed = SEED_BUTTON_STATE_LOCKED;
  for (int i = 0; i < 400; ++i) {
    clocked.Block(NULL);
    CHECK(clocked.processor.delay_controller_.test_timing_change_gain()
        == 1.0f);
  }
  CHECK(clocked.processor.delay_controller_.test_synchronized());

  // A latch released within the fade-out never switches timing.
  Rig rapid;
  PrepareTappedDelay(&rapid, Processor::MEDIA_DIGITAL, 0);
  rapid.seed = SEED_BUTTON_STATE_LOCKED;
  rapid.Run(20);
  const float dip = rapid.processor.delay_controller_.test_timing_change_gain();
  CHECK(dip > 0.0f && dip < 1.0f);
  rapid.seed = SEED_BUTTON_STATE_OFF;
  float lowest = dip;
  for (int i = 0; i < 100; ++i) {
    rapid.Block(NULL);
    CHECK(rapid.processor.delay_controller_.test_synchronized());
    lowest = std::min(
        lowest, rapid.processor.delay_controller_.test_timing_change_gain());
  }
  CHECK(lowest == dip);
  CHECK(rapid.processor.delay_controller_.test_timing_change_gain() == 1.0f);

  // The fade reaches silence before the switch and lasts 10 ms each way.
  Rig timing;
  PrepareTappedDelay(&timing, Processor::MEDIA_DIGITAL, 0);
  timing.seed = SEED_BUTTON_STATE_LOCKED;
  int switch_block = -1;
  int recovered_block = -1;
  for (int i = 0; i < 200 && recovered_block < 0; ++i) {
    timing.Block(NULL);
    const DelayController& c = timing.processor.delay_controller_;
    if (switch_block < 0 && !c.test_synchronized()) {
      CHECK(c.test_timing_change_gain() == 0.0f);
      switch_block = i;
    } else if (switch_block >= 0 && c.test_timing_change_gain() == 1.0f) {
      recovered_block = i;
    }
  }
  // Float rounding may leave one more block at either end.
  CHECK(switch_block >= 40 && switch_block <= 41);
  CHECK(recovered_block - switch_block >= 40);
  CHECK(recovered_block - switch_block <= 41);

}

void TestSeedLatchFadeTransitions() {
  test_checker.Start("TestSeedLatchFadeTransitions");
  // Leaving delay mode mid-fade, and re-entering, completes the fade.
  Rig exit;
  PrepareTappedDelay(&exit, Processor::MEDIA_DIGITAL, 0);
  exit.seed = SEED_BUTTON_STATE_LOCKED;
  exit.Run(20);
  const float exit_gain =
      exit.processor.delay_controller_.test_timing_change_gain();
  exit.parameters.size.base = 0.5f;
  exit.processor.delay_mode_transition_ = 0.0001f;
  exit.Run(1);
  CHECK(exit.processor.delay_mode_transition_ < 0.0f);
  CHECK(exit.processor.delay_controller_.test_timing_change_gain()
      < exit_gain);
  exit.parameters.size.base = 65535.0f / 65536.0f;
  exit.Run(90);
  CHECK(exit.processor.delay_mode_transition_ > 0.0f);
  CHECK(!exit.processor.delay_controller_.test_synchronized());
  CHECK(exit.processor.delay_controller_.test_timing_change_gain() == 1.0f);

  // A quality change mid-fade, frozen, in either direction, restarts in the
  // requested timing at once.
  for (int from_latched = 0; from_latched < 2; ++from_latched) {
    for (int latched = 0; latched < 2; ++latched) {
      Rig quality;
      PrepareTappedDelay(&quality, Processor::MEDIA_DIGITAL, 0);
      const DelayController& c = quality.processor.delay_controller_;
      quality.frozen = true;
      if (from_latched) {
        quality.seed = SEED_BUTTON_STATE_LOCKED;
        quality.Run(200);
        CHECK(!c.test_synchronized());
      }
      quality.seed = from_latched
          ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_LOCKED;
      quality.Run(20);
      CHECK(c.test_synchronized() == !from_latched);
      CHECK(c.test_timing_change_gain() > 0.4f);
      CHECK(c.test_timing_change_gain() < 0.6f);
      quality.seed = latched
          ? SEED_BUTTON_STATE_LOCKED : SEED_BUTTON_STATE_OFF;
      quality.processor.Configure(Processor::MEDIA_CLEAN_TAPE, false);
      CHECK(!c.test_timing_fade());
      quality.processor.set_seed_latch_fade(true);  // The next main loop pass.
      for (int i = 0; i < 400; ++i) {
        quality.Block(NULL);
        CHECK(c.test_synchronized() == !latched);
        CHECK(c.test_timing_change_gain() == 1.0f);
      }
    }
  }

  // A latch change made outside delay mode fades in on entry, while the
  // granular/delay crossfade still silences the wet signal.
  Rig entry;
  entry.processor.set_seed_latch_fade(true);
  entry.processor.delay_mode_transition_ = 1.5f;
  entry.Run(10);
  CHECK(!entry.processor.delay_controller_.test_synchronized());
  entry.seed = SEED_BUTTON_STATE_OFF;
  entry.processor.delay_mode_transition_ = -0.0001f;
  entry.Run(2);
  CHECK(entry.processor.delay_mode_transition_ > 0.0f);
  CHECK(entry.processor.delay_controller_.test_timing_change_gain() < 1.0f);
  entry.Run(90);
  CHECK(entry.processor.delay_mode_transition_ < 0.025f);
  CHECK(entry.processor.delay_controller_.test_synchronized());
  CHECK(entry.processor.delay_controller_.test_timing_change_gain() == 1.0f);
}

// Off, the profile default, keeps the stock switch and its pop.
void TestSeedLatchFadeOff() {
  test_checker.Start("TestSeedLatchFadeOff");
  float worst = 0.0f;
  for (int media = 0; media < 4; ++media) {
    for (int offset = 0; offset < 300; offset += 101) {
      Rig r;
      r.latch_fade = false;
      PrepareTappedDelay(&r, media, offset);
      const float latch = SeedStepRatio(&r, SEED_BUTTON_STATE_LOCKED, NULL);
      const float unlatch = SeedStepRatio(&r, SEED_BUTTON_STATE_ON, NULL);
      worst = std::max(worst, std::max(latch, unlatch));
      // The fading path never ran.
      CHECK(!r.processor.delay_controller_.test_timing_fade());
    }
  }
  printf("Off latch/unlatch step ratio: %.3f\n", worst);
  CHECK(worst > 4.0f);
}

void TestSeedLatchFadeTaps() {
  test_checker.Start("TestSeedLatchFadeTaps");
  // SEED edges during the fade-out toward tapped timing are not recorded,
  // and the tap counter only runs while tapped timing is rendered.
  Rig edges;
  PrepareTappedDelay(&edges, Processor::MEDIA_DIGITAL, 0);
  edges.seed = SEED_BUTTON_STATE_LOCKED;
  edges.Run(200);
  const DelayController& e = edges.processor.delay_controller_;
  CHECK(!e.test_synchronized());
  DelayClock* clock = const_cast<DelayClock*>(&e.test_clock());
  const int counter = clock->test_next_interval();
  edges.seed = SEED_BUTTON_STATE_ON;
  edges.Run(4);
  edges.seed = SEED_BUTTON_STATE_OFF;
  edges.Run(4);
  edges.seed = SEED_BUTTON_STATE_ON;
  int synchronized_blocks = 0;
  for (int i = 0; i < 60; ++i) {
    edges.Block(NULL);
    synchronized_blocks += e.test_synchronized();
  }
  CHECK(synchronized_blocks > 0 && synchronized_blocks < 60);
  CHECK(clock->test_next_interval() - counter == 12 * synchronized_blocks);
  // The first accepted tap records the interval spanning the switch.
  edges.seed = SEED_BUTTON_STATE_OFF;
  edges.Run(100);
  const int spanning = clock->test_next_interval() + 1;
  edges.seed = SEED_BUTTON_STATE_ON;
  edges.Run(1);
  CHECK(clock->interval(clock->new_interval_index()) == float(spanning));

  // Taps after a latch cycle record the same interval as without one.
  Rig plain;
  Rig cycled;
  PrepareTappedDelay(&plain, Processor::MEDIA_DIGITAL, 0);
  PrepareTappedDelay(&cycled, Processor::MEDIA_DIGITAL, 0);
  plain.Run(4400);
  cycled.seed = SEED_BUTTON_STATE_LOCKED;
  cycled.Run(2000);
  cycled.seed = SEED_BUTTON_STATE_ON;
  cycled.Run(400);
  cycled.seed = SEED_BUTTON_STATE_OFF;
  cycled.Run(2000);
  for (int tap = 0; tap < 3; ++tap) {
    plain.seed = cycled.seed = SEED_BUTTON_STATE_ON;
    plain.Run(400);
    cycled.Run(400);
    plain.seed = cycled.seed = SEED_BUTTON_STATE_OFF;
    plain.Run(1200);
    cycled.Run(1200);
  }
  const DelayClock& a = plain.processor.delay_controller_.test_clock();
  const DelayClock& b = cycled.processor.delay_controller_.test_clock();
  printf("Tapped interval, plain / after latch: %.1f / %.1f\n",
      a.interval(a.new_interval_index()), b.interval(b.new_interval_index()));
  CHECK(fabsf(a.interval(a.new_interval_index()) - 19200.0f) < 20.0f);
  CHECK(fabsf(b.interval(b.new_interval_index()) - 19200.0f) < 20.0f);
}

// Knob-timing delay with FEEDBACK at the unity notch and the delay time
// set by DENSITY; SHAPE fully counter-clockwise keeps the taps flat.
void PrepareKnobDelay(Rig* r, int media, bool unity, float density) {
  r->processor.Configure(Processor::Media(media), false);
  r->processor.set_unity_feedback(unity);
  r->parameters.shape.base = 0.0f;
  r->density.base = density;
  r->mix.feedback = 0.0f;
  r->amplitude = 0.1f;
  r->processor.delay_mode_transition_ = 1.5f;
  r->Run(2000);
}

float KnobInterval(const Rig& r) {
  const DelayClock& c = r.processor.delay_controller_.test_clock();
  return c.interval(c.new_interval_index());
}

// Silent input for one delay time and a bit, so no earlier tone recirculates.
void Flush(Rig* r) {
  r->amplitude = 0.0f;
  r->Run(int(KnobInterval(*r) * 4000.0f / r->processor.sample_rate()) + 100);
}

// Average gain per pass, in dB, of a tone burst echoing round the loop.
float PassGain(int media, bool unity, float feedback, float frequency,
    float density, int passes) {
  Rig r;
  PrepareKnobDelay(&r, media, unity, density);
  const float period = KnobInterval(r);
  Flush(&r);
  r.mix.feedback = feedback;
  r.amplitude = 0.1f;
  r.frequency = frequency;
  r.burst_start = r.sample;
  r.burst_length = int(period / 4.0f);
  std::vector<float> audio;
  const float block_size = r.processor.sample_rate() / 4000.0f;
  const int blocks = int((passes + 2) * period / block_size) + 10;
  for (int i = 0; i < blocks; ++i) {
    r.Block(&audio);
  }
  double energy[2] = { 0.0, 0.0 };
  for (int k = 0; k < 2; ++k) {
    const int centre = int((k ? passes : 1) * period + period / 8.0f);
    for (int n = centre - int(period / 2.0f); n < centre + int(period / 2.0f);
         ++n) {
      energy[k] += double(audio[n]) * audio[n];
    }
  }
  CHECK(energy[0] > 1e-6);
  return float(10.0 * log10(energy[1] / energy[0]) / (passes - 1));
}

void TestUnityFeedbackLaw() {
  test_checker.Start("TestUnityFeedbackLaw");
  for (int taps = 0; taps < 3; ++taps) {
    const float sum = 1.0f + 0.3f * float(taps);
    const float unity = Processor::UnityFeedback(0.8165f, sum);
    // The loop gain of the notch is one, for one tap or two.
    CHECK(fabsf(1.5f * unity * unity * sum - 1.0f) < 1e-5f);
    CHECK(Processor::UnityFeedback(0.0f, sum) == 0.0f);
    CHECK(fabsf(Processor::UnityFeedback(1.0f, sum) - 1.0f) < 1e-6f);
    // A notch 6% of the travel wide, at stock unity.
    CHECK(Processor::UnityFeedback(0.7875f, sum) == unity);
    CHECK(Processor::UnityFeedback(0.8455f, sum) == unity);
    CHECK(Processor::UnityFeedback(0.7855f, sum) < unity);
    CHECK(Processor::UnityFeedback(0.8475f, sum) > unity);
    float previous = 0.0f;
    float largest_step = 0.0f;
    for (int i = 0; i <= 1000; ++i) {
      const float f = Processor::UnityFeedback(float(i) / 1000.0f, sum);
      CHECK(f >= previous);
      largest_step = std::max(largest_step, f - previous);
      previous = f;
    }
    // Continuous: no step larger than the steepest slope allows.
    CHECK(largest_step < 0.0035f);
  }
}

void TestUnityFeedbackLoop() {
  test_checker.Start("TestUnityFeedbackLoop");
  // One tap: DENSITY 0.15 gives about 50 ms on the digital qualities, 0.258
  // about 0.2 s, long enough for a 100 Hz burst.
  for (int media = 0; media < 4; ++media) {
    const float notch = PassGain(media, true, 0.82f, 1000.0f, 0.15f, 12);
    const float bass = PassGain(media, true, 0.82f, 100.0f, 0.258f, 8);
    const float stock_bass = PassGain(media, false, 0.8165f, 100.0f, 0.258f, 8);
    const float top = PassGain(media, true, 1.0f, 1000.0f, 0.15f, 6);
    printf("Quality %d per pass: 1 kHz %+.3f dB, 100 Hz %+.3f dB "
        "(stock %+.3f dB), full CW %+.3f dB\n",
        media, notch, bass, stock_bass, top);
    CHECK(notch < 0.01f && notch > -0.3f);
    CHECK(bass < 0.01f && bass > -0.3f);
    CHECK(stock_bass < -0.8f);
    // Fully clockwise keeps stock's runaway.
    CHECK(top > 0.5f);
  }
}

// Loop level after a burst, with silent input: the first second and the
// fifth.
void LoopLevels(bool unity, float density, int media, float* levels) {
  Rig r;
  PrepareKnobDelay(&r, media, unity, density);
  Flush(&r);
  r.mix.feedback = 0.82f;
  r.amplitude = 0.05f;
  r.frequency = 300.0f;
  r.burst_start = r.sample;
  r.burst_length = int(r.processor.sample_rate() * 0.01f);
  std::vector<float> first;
  std::vector<float> last;
  for (int i = 0; i < 4000; ++i) {
    r.Block(&first);
  }
  r.Run(12000);
  for (int i = 0; i < 4000; ++i) {
    r.Block(&last);
  }
  levels[0] = Rms(first);
  levels[1] = Rms(last);
}

void TestUnityFeedbackTwoTaps() {
  test_checker.Start("TestUnityFeedbackTwoTaps");
  // DENSITY 0.9 adds the odd tap; the notch scales down to hold unity.
  for (int media = 0; media < 4; ++media) {
    float two[2];
    float stock[2];
    float one[2];
    LoopLevels(true, 0.9f, media, two);
    LoopLevels(false, 0.9f, media, stock);
    LoopLevels(true, 0.1f, media, one);
    const float two_db = 20.0f * log10f(std::max(two[1], 1e-9f) / two[0]);
    const float one_db = 20.0f * log10f(one[1] / one[0]);
    printf("Quality %d over 4 s: two taps %+.2f dB, one tap %+.2f dB; "
        "fifth second RMS %.4f, stock %.4f\n",
        media, two_db, one_db, two[1], stock[1]);
    CHECK(two[0] > 0.001f && one[0] > 0.001f);
    // Two taps at different delays fall below unity between their peaks.
    CHECK(two_db < 0.5f);
    CHECK(one_db < 0.5f && one_db > -6.0f);
    // At the same knob position stock grows into the limiter.
    CHECK(stock[1] > 10.0f * two[1]);
  }
}

// DENSITY jumping across noon every two blocks adds and drops the odd tap
// faster than the feedback follows.
void TestUnityFeedbackDensitySteps() {
  test_checker.Start("TestUnityFeedbackDensitySteps");
  for (int media = 0; media < 4; media += 3) {
    Rig r;
    PrepareKnobDelay(&r, media, true, 0.1f);
    Flush(&r);
    r.mix.feedback = 0.82f;
    r.amplitude = 0.05f;
    r.frequency = 300.0f;
    r.burst_start = r.sample;
    r.burst_length = int(r.processor.sample_rate() * 0.01f);
    const DelayController& d = r.processor.delay_controller_;
    const float* feedback = media
        ? &r.processor.processor_digital_.feedback_
        : &r.processor.processor_dirty_tape_.feedback_;
    float peak[2] = { 0.0f, 0.0f };
    float worst = 0.0f;
    for (int i = 0; i < 16000; ++i) {
      r.density.base = (i / 2) % 2 ? 0.9f : 0.1f;
      const float start = d.tap_gain(false);
      std::vector<float> audio;
      r.Block(&audio);
      // The block's feedback target holds unity for the taps at both ends.
      const float taps = std::max(start, d.tap_gain(false));
      worst = std::max(worst, 1.5f * *feedback * *feedback * taps);
      for (size_t n = 0; n < audio.size(); ++n) {
        peak[i >= 8000] = std::max(peak[i >= 8000], fabsf(audio[n]));
      }
    }
    printf("Quality %d DENSITY steps: peak %.4f, then %.4f; "
        "largest end-of-block loop gain %.5f\n",
        media, peak[0], peak[1], worst);
    CHECK(peak[0] > 0.01f);
    CHECK(peak[1] <= peak[0]);
    CHECK(worst < 1.0001f);
  }
}

void TestUnityFeedbackScope() {
  test_checker.Start("TestUnityFeedbackScope");
  // Granular playback is unchanged, and leaving delay restores 40 Hz.
  for (int media = 0; media < 4; ++media) {
    std::vector<float> audio[2];
    float g[2];
    for (int unity = 0; unity < 2; ++unity) {
      Rig r;
      r.processor.Configure(Processor::Media(media), false);
      r.processor.set_unity_feedback(unity);
      r.parameters.size.base = 0.5f;
      r.mix.feedback = 0.82f;
      stmlib::Random::Seed(7);
      for (int i = 0; i < 3000; ++i) {
        r.Block(&audio[unity]);
      }
      CHECK(!(r.processor.options_ & Processor::OPTION_FLAG_LOW_CUTOFF));
      g[unity] = r.processor.processor_clean_tape_.dc_blocker_[1].g_;
    }
    CHECK(audio[0] == audio[1]);
    CHECK(g[0] == g[1]);
  }
  Rig r;
  const float stock = r.processor.processor_clean_tape_.dc_blocker_[0].g_;
  PrepareKnobDelay(&r, Processor::MEDIA_CLEAN_TAPE, true, 0.3f);
  CHECK((r.processor.options_ & Processor::OPTION_FLAG_LOW_CUTOFF));
  CHECK(r.processor.processor_clean_tape_.dc_blocker_[0].g_ < 0.3f * stock);
  r.parameters.size.base = 0.5f;
  r.Run(6100);
  CHECK(!(r.processor.options_ & Processor::OPTION_FLAG_LOW_CUTOFF));
  CHECK(r.processor.processor_clean_tape_.dc_blocker_[0].g_ == stock);
  r.parameters.size.base = 65535.0f / 65536.0f;
  r.Run(6100);
  CHECK((r.processor.options_ & Processor::OPTION_FLAG_LOW_CUTOFF));
  r.processor.set_unity_feedback(false);
  CHECK(!(r.processor.options_ & Processor::OPTION_FLAG_LOW_CUTOFF));
  CHECK(r.processor.processor_clean_tape_.dc_blocker_[0].g_ == stock);
  // Clocked timing has one tap's gain in all; the notch is not scaled.
  const DelayController& d = r.processor.delay_controller_;
  CHECK(d.tap_gain(true) == 1.0f && d.tap_gain(false) == 1.0f);
  r.density.base = 0.6f;
  // The odd tap's gain swings slowly between a quarter and full.
  float lowest = 2.0f;
  float highest = 0.0f;
  for (int i = 0; i < 24000; ++i) {
    r.Block(NULL);
    lowest = std::min(lowest, d.tap_gain(false));
    highest = std::max(highest, d.tap_gain(false));
  }
  CHECK(fabsf(lowest - 1.15f) < 0.01f && fabsf(highest - 1.6f) < 0.01f);
  CHECK(d.tap_gain(true) == 1.0f);
}

// Pitch deviation, in cents, of a delayed 750 Hz sine, and the level dip.
struct Warble {
  float cents;
  float dip;
  int cycles;
  float last;
};

Warble MeasureWarble(Rig* r, float to, int ramp_blocks) {
  r->amplitude = 0.1f;
  r->frequency = 750.0f;
  r->Run(4000);
  const float from = r->density.base;
  std::vector<float> audio;
  for (int i = 0; i < 12000; ++i) {
    const float x = std::min(1.0f, float(i) / float(ramp_blocks + 1));
    r->density.base = from + (to - from) * x;
    r->Block(&audio);
  }
  Warble w = { 0.0f, 0.0f, 0, 0.0f };
  const double sr = r->processor.sample_rate();
  double previous = -1.0;
  float peak = 0.0f;
  for (size_t n = 1; n < audio.size(); ++n) {
    peak = std::max(peak, fabsf(audio[n]));
    if (audio[n - 1] < 0.0f && audio[n] >= 0.0f) {
      const double t = n - 1 + audio[n - 1] / (audio[n - 1] - audio[n]);
      // Phase slips at a crossfade's dip are not pitch: skip cycles 12 dB down.
      if (previous >= 0.0 && peak > 0.025f) {
        const double cents = 1200.0 * log2(sr / (t - previous) / 750.0);
        w.cents = std::max(w.cents, float(fabs(cents)));
        ++w.cycles;
      }
      previous = t;
      peak = 0.0f;
    }
  }
  // Level in 4 ms windows.
  const size_t length = size_t(sr * 0.004);
  float lowest = 1.0f;
  for (size_t n = 0; n + length <= audio.size(); n += length) {
    std::vector<float> window(&audio[n], &audio[n + length]);
    lowest = std::min(lowest, Rms(window));
    w.last = Rms(window);
  }
  w.dip = 20.0f * log10f(std::max(lowest, 1e-6f) / (0.1f / sqrtf(2.0f)));
  return w;
}

void TestDelayCrossfade() {
  test_checker.Start("TestDelayCrossfade");
  // DENSITY steps and a one second sweep, digital and tape.
  const int ramps[] = { 0, 4000 };
  for (int media = 0; media < 4; media += 1) {
    for (int ramp = 0; ramp < 2; ++ramp) {
      Warble w[2];
      for (int crossfade = 0; crossfade < 2; ++crossfade) {
        Rig r;
        PrepareKnobDelay(&r, media, false, 0.258f);
        r.processor.set_delay_crossfade(crossfade);
        r.mix.feedback = 0.0f;
        w[crossfade] = MeasureWarble(&r, 0.3f, ramps[ramp]);
      }
      printf("Quality %d %s: glide %.0f c (%+.1f dB), "
          "crossfade %.0f c (%+.1f dB)\n", media, ramp ? "sweep" : "step",
          w[0].cents, w[0].dip, w[1].cents, w[1].dip);
      CHECK(w[0].cents > 200.0f);
      // Most of the 2,250 cycles are measured and the level comes back.
      CHECK(w[1].cycles > 1500);
      CHECK(w[1].last > 0.5f * 0.1f / sqrtf(2.0f));
      CHECK(w[1].cents < 0.4f * w[0].cents);
      CHECK(w[1].cents < (ramp ? 300.0f : 100.0f));
    }
  }
}

void TestDelayCrossfadeClock() {
  test_checker.Start("TestDelayCrossfadeClock");
  Rig r;
  PrepareKnobDelay(&r, Processor::MEDIA_DIGITAL, false, 0.258f);
  r.processor.set_delay_crossfade(true);
  r.Run(4000);
  const DelayClock& c = r.processor.delay_controller_.test_clock();
  const float from = KnobInterval(r);
  CHECK(c.fade() == 1.0f);
  // A 1% change is ignored.
  r.density.base = 0.2585f;
  r.Run(400);
  CHECK(KnobInterval(r) == from);
  // A step fades over half the new interval; the old one fades out.
  r.density.base = 0.3f;
  r.Run(40);
  const float to = KnobInterval(r);
  printf("Crossfade from %.1f to %.1f samples\n", from, to);
  CHECK(to > 1.2f * from);
  CHECK(c.fade() > 0.0f && c.fade() < 1.0f);
  CHECK(c.interval(1 - c.new_interval_index()) == from);
  const int expected = int(std::min(0.25f * 48000.0f,
      std::max(0.02f * 48000.0f, to * 0.5f)) / 12.0f);
  // A change during the fade waits for its end, then fades from there.
  r.density.base = 0.32f;
  int blocks = 40;
  while (KnobInterval(r) == to && blocks < 10000) {
    r.Block(NULL);
    ++blocks;
  }
  CHECK(abs(blocks - expected) <= 1);
  CHECK(KnobInterval(r) > to);
  CHECK(c.interval(1 - c.new_interval_index()) == to);
  // Under 10 ms the delay glides, as stock, for flanging.
  Rig flange;
  PrepareKnobDelay(&flange, Processor::MEDIA_DIGITAL, false, 0.0f);
  flange.processor.set_delay_crossfade(true);
  flange.Run(4000);
  const DelayClock& f = flange.processor.delay_controller_.test_clock();
  const float short_delay = KnobInterval(flange);
  CHECK(short_delay < 480.0f);
  flange.density.base = 0.01f;
  bool between = false;
  for (int i = 0; i < 200; ++i) {
    flange.Block(NULL);
    const float now = KnobInterval(flange);
    between = between || (now > short_delay + 1.0f && now < 420.0f);
    CHECK(f.fade() == 1.0f);
  }
  CHECK(between);
  CHECK(KnobInterval(flange) > short_delay + 10.0f);
  // Clocked timing is untouched.
  std::vector<float> audio[2];
  for (int crossfade = 0; crossfade < 2; ++crossfade) {
    Rig clocked;
    PrepareTappedDelay(&clocked, Processor::MEDIA_DIGITAL, 0);
    clocked.processor.set_delay_crossfade(crossfade);
    for (int i = 0; i < 4000; ++i) {
      clocked.density.base = i < 2000 ? 0.3f : 0.45f;
      clocked.parameters.time.base = i < 3000 ? 0.0f : 0.4f;
      clocked.Block(&audio[crossfade]);
    }
  }
  CHECK(audio[0] == audio[1]);
}

void TestDelayCrossfadeLatchFade() {
  test_checker.Start("TestDelayCrossfadeLatchFade");
  // Knob timing behind the SEED latch fade crossfades too.
  Rig r;
  PrepareTappedDelay(&r, Processor::MEDIA_DIGITAL, 0);
  r.processor.set_delay_crossfade(true);
  r.seed = SEED_BUTTON_STATE_LOCKED;
  r.Run(4000);
  const DelayController& d = r.processor.delay_controller_;
  CHECK(d.test_timing_fade() && !d.test_synchronized());
  const DelayClock& c = d.test_clock();
  const float from = KnobInterval(r);
  r.density.base = 0.35f;
  r.Run(20);
  CHECK(c.fade() < 1.0f);
  CHECK(c.interval(1 - c.new_interval_index()) == from);
  // Frozen slices follow the crossfade too.
  r.frozen = true;
  r.Run(4000);
  r.density.base = 0.3f;
  r.Run(20);
  CHECK(c.fade() < 1.0f);
}

// Knob-timing renders with FEEDBACK, DENSITY and FREEZE moves, captured from
// the build before these options; Off must stay bit-identical.
uint32_t KnobDelayHash(int media, bool mono) {
  Rig r;
  r.processor.Configure(Processor::Media(media), mono);
  r.processor.delay_mode_transition_ = 1.5f;
  r.mix.feedback = 0.82f;
  r.mix.dry_wet = 0.7f;
  r.mix.reverb_amount = 0.3f;
  r.amplitude = 0.3f;
  r.frequency = 330.0f;
  stmlib::Random::Seed(11);
  uint32_t hash = 2166136261U;
  std::vector<float> audio;
  for (int i = 0; i < 12000; ++i) {
    r.density.base = i < 4000 ? 0.258f : (i < 6000 ? 0.3f : 0.6f);
    r.density.base += i >= 8000 ? 0.00001f * float(i - 8000) : 0.0f;
    r.mix.feedback = i < 9000 ? 0.82f : 0.95f;
    r.frozen = i >= 10000 && i < 11000;
    r.Block(&audio);
  }
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&audio[0]);
  for (size_t i = 0; i < audio.size() * sizeof(float); ++i) {
    hash = (hash ^ bytes[i]) * 16777619U;
  }
  return hash;
}

void TestKnobDelayHashes() {
  test_checker.Start("TestKnobDelayHashes");
  const uint32_t expected[4][2] = {
    { 0x10ec0c35, 0x20590c8a },
    { 0x5a58aee5, 0x4ee86ad8 },
    { 0x251fdc07, 0x0ea9104d },
    { 0x1716b800, 0xe0f81bf3 }
  };
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      const uint32_t hash = KnobDelayHash(media, mono);
      printf("Knob delay q%d mono%d %08x\n", media, mono, hash);
      CHECK(hash == expected[media][mono]);
    }
  }
}

int main(int argc, char** argv) {
  setbuf(stdout, NULL);
  const HostTest tests[] = {
    { "TestDelayFilterRamp", TestDelayFilterRamp },
    { "TestDelayModeSwitchClick", TestDelayModeSwitchClick },
    { "TestDelayOptions", TestDelayOptions },
    { "TestContinuousDelayPlayback", TestContinuousDelayPlayback },
    { "TestQuantizerDelayBypassDigital", TestQuantizerDelayBypassDigital },
    { "TestQuantizerDelayBypassTape", TestQuantizerDelayBypassTape },
    { "TestDelayModeFade", TestDelayModeFade },
    { "TestDelayRotaryPitch", TestDelayRotaryPitch },
    { "TestDelayBandwidthAndCv", TestDelayBandwidthAndCv },
    { "TestSeedLatchFade", TestSeedLatchFade },
    { "TestSeedLatchFadePolicy", TestSeedLatchFadePolicy },
    { "TestSeedLatchFadeTransitions", TestSeedLatchFadeTransitions },
    { "TestSeedLatchFadeTaps", TestSeedLatchFadeTaps },
    { "TestSeedLatchFadeOff", TestSeedLatchFadeOff },
    { "TestUnityFeedbackLaw", TestUnityFeedbackLaw },
    { "TestUnityFeedbackLoop", TestUnityFeedbackLoop },
    { "TestUnityFeedbackTwoTaps", TestUnityFeedbackTwoTaps },
    { "TestUnityFeedbackDensitySteps", TestUnityFeedbackDensitySteps },
    { "TestUnityFeedbackScope", TestUnityFeedbackScope },
    { "TestDelayCrossfade", TestDelayCrossfade },
    { "TestDelayCrossfadeClock", TestDelayCrossfadeClock },
    { "TestDelayCrossfadeLatchFade", TestDelayCrossfadeLatchFade },
    { "TestKnobDelayHashes", TestKnobDelayHashes },
  };
  const int result = RunHostTests(argc, argv, tests,
      sizeof(tests) / sizeof(tests[0]));
  return result ? result : (test_checker.failures() ? 1 : 0);
}

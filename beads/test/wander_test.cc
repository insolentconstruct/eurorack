// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Wander and Showers: random grain values and timing with memory.

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
static stmlib::GateFlags seed[12];

void SetParameter(RandomizedParameter* p, float base, float amount, bool cv) {
  p->base = base;
  p->attenuverter = amount;
  p->cv = cv ? 0.3f : 0.0f;
  p->has_cv = cv;
}

void Init(Processor::Media media, int options) {
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  memset(banks, 0, sizeof(banks));
  memset(reverb_buffer, 0, sizeof(reverb_buffer));
  uint8_t* buffers[3] = { banks[0], banks[1], banks[2] };
  processor.Init(buffers, reverb_buffer);
  processor.Configure(media, false);
  processor.options_ = options;
  memset(&parameters, 0, sizeof(parameters));
  memset(&density, 0, sizeof(density));
  memset(&mix, 0, sizeof(mix));
  SetParameter(&parameters.time, 0.3f, 0.0f, false);
  SetParameter(&parameters.size, 0.4f, 0.0f, false);
  SetParameter(&parameters.shape, 0.5f, 0.0f, false);
  SetParameter(&parameters.pitch, 0.0f, 0.0f, false);
  density.base = 0.8f;
  mix.dry_wet = 1.0f;
  mix.reverb_amount = 0.3f;
  mix.feedback = 0.2f;
  mix.output_gate = true;
  stmlib::Random::Seed(1);
}

// Renders and hashes the output; counts grains from the gate output.
uint64_t Render(int blocks, SeedButtonState state, bool clocked, int* grains,
    float* longest_gap, int* window_counts, int window_blocks) {
  const size_t size = size_t(processor.sample_rate()) / 4000;
  uint64_t hash = 14695981039346656037ull;
  bool previous = false;
  stmlib::GateFlags flags = stmlib::GATE_FLAG_LOW;
  int count = 0;
  int gap = 0;
  int longest = 0;
  for (int block = 0; block < blocks; ++block) {
    for (size_t i = 0; i < size; ++i) {
      const float t = float(block * size + i);
      input[i].l = 0.3f * sinf(t * 0.031f) * (block % 900 < 450);
      input[i].r = 0.2f * sinf(t * 0.017f);
      freeze[i] = false;
      // Four clock pulses a second when clocked.
      const bool high = clocked && (block % 1000) < 10;
      seed[i] = flags = stmlib::ExtractGateFlags(flags, high);
    }
    processor.Buffer();
    processor.Process(state, clocked ? seed : NULL, freeze, density,
        parameters, mix, input, output, size);
    for (size_t i = 0; i < size; ++i) {
      uint32_t v[2];
      memcpy(&v[0], &output[i].l, 4);
      memcpy(&v[1], &output[i].r, 4);
      hash = (hash ^ v[0]) * 1099511628211ull;
      hash = (hash ^ v[1]) * 1099511628211ull;
      const bool gate = output[i].r != 0.0f;
      if (gate && !previous) {
        ++count;
        if (window_counts) {
          ++window_counts[block / window_blocks];
        }
        longest = std::max(longest, gap);
        gap = 0;
      }
      ++gap;
      previous = gate;
    }
  }
  if (grains) {
    *grains = count;
  }
  if (longest_gap) {
    *longest_gap = float(longest) / processor.sample_rate();
  }
  return hash;
}

void TestWanderWalk() {
  test_checker.Start("TestWanderWalk");
  for (int intervals = 0; intervals < 2; ++intervals) {
    Wander wander;
    wander.Init();
    stmlib::Random::Seed(7);
    float previous_time = 0.0f;
    float previous_pitch = 0.0f;
    float lowest = 1.0f;
    float highest = -1.0f;
    float lag = 0.0f;
    float energy = 0.0f;
    for (int grain = 0; grain < 4000; ++grain) {
      Parameters p;
      SetParameter(&p.time, 0.3f, -0.5f, false);
      SetParameter(&p.size, 0.4f, 0.4f, false);
      SetParameter(&p.shape, 0.5f, -0.5f, true);
      SetParameter(&p.pitch, 0.0f, -1.0f, false);
      p.monophonic_mode = false;
      Parameters untouched = p;
      const float spread = wander.Process(true, intervals, &p);
      CHECK(spread == 1.5f);
      // Clockwise and patched parameters stay as they were.
      CHECK(!memcmp(&p.size, &untouched.size, sizeof(p.size)));
      CHECK(!memcmp(&p.shape, &untouched.shape, sizeof(p.shape)));
      CHECK(p.time.has_cv && p.time.attenuverter == 1.0f);
      CHECK(p.pitch.has_cv && p.pitch.attenuverter == 1.0f);
      // TIME blends towards a point in [0, 1] at half the knob's amount.
      const float time = p.time.value();
      CHECK(time >= 0.3f - 0.5f * 0.3f - 1e-6f);
      CHECK(time <= 0.3f + 0.5f * 0.7f + 1e-6f);
      CHECK(fabsf(p.pitch.cv) <= 24.0f + 1e-4f);
      if (intervals) {
        CHECK(QuantizeFifths(p.pitch.cv) == p.pitch.cv);
      }
      if (grain) {
        // One grain's step is at most half the span, a quarter on TIME.
        CHECK(fabsf(time - previous_time) <= 0.5f * 0.5f * 0.5f + 1e-5f);
        if (!intervals) {
          CHECK(fabsf(p.pitch.cv - previous_pitch) <= 12.0f + 1e-4f);
        }
        lag += p.pitch.cv * previous_pitch;
      }
      energy += p.pitch.cv * p.pitch.cv;
      lowest = std::min(lowest, time);
      highest = std::max(highest, time);
      previous_time = time;
      previous_pitch = p.pitch.cv;
      // Without a new grain the offset holds.
      Parameters again;
      SetParameter(&again.time, 0.3f, -0.5f, false);
      SetParameter(&again.size, 0.4f, 0.4f, false);
      SetParameter(&again.shape, 0.5f, -0.5f, true);
      SetParameter(&again.pitch, 0.0f, -1.0f, false);
      wander.Process(false, intervals, &again);
      CHECK(again.time.cv == p.time.cv && again.pitch.cv == p.pitch.cv);
    }
    // It roams the whole span, one small step at a time.
    CHECK(highest - lowest > 0.4f);
    CHECK(lag / energy > 0.9f);
  }
  // Stock counter-clockwise draws are independent from grain to grain.
  stmlib::Random::Seed(7);
  RandomizedParameter stock;
  SetParameter(&stock, 0.0f, -1.0f, false);
  float lag = 0.0f;
  float energy = 0.0f;
  float previous = 0.0f;
  for (int grain = 0; grain < 4000; ++grain) {
    const float pitch = stock.value(NULL, 0, 24.0f, false);
    lag += pitch * previous;
    energy += pitch * pitch;
    previous = pitch;
  }
  CHECK(fabsf(lag / energy) < 0.1f);
}

void TestWanderKeepsPanSpread() {
  test_checker.Start("TestWanderKeepsPanSpread");
  Init(Processor::MEDIA_DIGITAL, 0);
  Wander wander;
  wander.Init();
  for (int amount = 0; amount < 8; ++amount) {
    Parameters p;
    SetParameter(&p.time, 0.3f, -0.1f * amount, false);
    SetParameter(&p.size, 0.4f, 0.05f * amount, false);
    SetParameter(&p.shape, 0.5f, -0.1f, amount & 1);
    SetParameter(&p.pitch, 0.0f, -0.08f * amount, false);
    p.monophonic_mode = false;
    TriggerGenerator::Output trigger;
    trigger.trigger = true;
    trigger.gate = true;
    trigger.fractional_sample = 0.0f;
    trigger.regularity = amount & 2 ? 0.0f : 1.0f;
    trigger.phase = 0.0f;
    const float before = processor.processor_digital_.granular_player_.
        ComputeSpread(trigger, p);
    trigger.regularity -= wander.Process(true, false, &p);
    const float after = processor.processor_digital_.granular_player_.
        ComputeSpread(trigger, p);
    CHECK(fabsf(before - after) < 1e-6f);
  }
}

void TestOnlyTheirOwnRanges() {
  test_checker.Start("TestOnlyTheirOwnRanges");
  // Each case: output with the option on must match the option off.
  for (int scenario = 0; scenario < 8; ++scenario) {
    uint64_t hashes[2];
    for (int on = 0; on < 2; ++on) {
      const int option = scenario < 3 || scenario == 7
          ? Processor::OPTION_WANDER : Processor::OPTION_SHOWERS;
      Init(scenario == 2 ? Processor::MEDIA_CLEAN_TAPE
          : Processor::MEDIA_DIGITAL, on ? option : 0);
      SeedButtonState state = SEED_BUTTON_STATE_LOCKED;
      bool clocked = false;
      switch (scenario) {
        case 0:
          // Wander: clockwise attenurandomizers.
          SetParameter(&parameters.time, 0.3f, 0.6f, false);
          SetParameter(&parameters.pitch, 0.0f, 0.5f, false);
          break;
        case 1:
          // Wander: counter-clockwise with CV patched.
          SetParameter(&parameters.time, 0.3f, -0.6f, true);
          SetParameter(&parameters.pitch, 0.0f, -0.5f, true);
          break;
        case 2:
          // Wander: delay mode keeps its slow random LFOs.
          SetParameter(&parameters.time, 0.3f, -0.6f, false);
          SetParameter(&parameters.size, 1.0f, 0.0f, false);
          processor.delay_mode_transition_ = 1.5f;
          break;
        case 3:
          // Showers: constant-rate DENSITY.
          density.base = 0.2f;
          break;
        case 4:
          // Showers: a clock in SEED, DENSITY as probability.
          density.base = 0.8f;
          clocked = true;
          break;
        case 5:
          // Showers: spectral.
          processor.set_spectral(true);
          break;
        case 6:
          // Showers: DENSITY at noon, taps on SEED.
          density.base = 0.5f;
          state = SEED_BUTTON_STATE_OFF;
          clocked = true;
          break;
        case 7:
          // Wander: spectral keeps its own randomness.
          SetParameter(&parameters.time, 0.3f, -0.6f, false);
          SetParameter(&parameters.pitch, 0.0f, -0.5f, false);
          processor.set_spectral(true);
          break;
      }
      hashes[on] = Render(6000, state, clocked, NULL, NULL, NULL, 1);
    }
    if (hashes[0] != hashes[1]) {
      printf("scenario %d differs\n", scenario);
    }
    CHECK(hashes[0] == hashes[1]);
  }
  // Negative controls: each option changes its own range.
  for (int option = 0; option < 2; ++option) {
    uint64_t hashes[2];
    for (int on = 0; on < 2; ++on) {
      Init(Processor::MEDIA_DIGITAL, on
          ? (option ? Processor::OPTION_SHOWERS : Processor::OPTION_WANDER)
          : 0);
      SetParameter(&parameters.time, 0.3f, -0.6f, false);
      hashes[on] = Render(6000, SEED_BUTTON_STATE_LOCKED, false, NULL, NULL,
          NULL, 1);
    }
    CHECK(hashes[0] != hashes[1]);
  }
}

void TestShowersRate() {
  test_checker.Start("TestShowersRate");
  // 240 s at 24 kHz; grains counted in 250 ms windows.
  const int blocks = 4000 * 240;
  const int window = 1000;
  const int windows = blocks / window;
  float mean[2];
  float fano[2];
  float gap[2];
  for (int on = 0; on < 2; ++on) {
    Init(Processor::MEDIA_CLEAN_TAPE, on ? Processor::OPTION_SHOWERS : 0);
    SetParameter(&parameters.size, 0.1f, 0.0f, false);
    density.base = 0.8f;
    static int counts[960];
    std::fill(counts, counts + windows, 0);
    int grains = 0;
    Render(blocks, SEED_BUTTON_STATE_LOCKED, false, &grains, &gap[on], counts,
        window);
    float sum = 0.0f;
    float square = 0.0f;
    for (int i = 0; i < windows; ++i) {
      sum += counts[i];
      square += float(counts[i]) * counts[i];
    }
    mean[on] = sum / windows;
    fano[on] = (square / windows - mean[on] * mean[on]) / mean[on];
    printf("showers %d: %d grains, %.2f per window, Fano %.2f, gap %.2f s\n",
        on, grains, mean[on], fano[on], gap[on]);
  }
  // Same mean rate, much more clustered, and the gaps stay bounded.
  CHECK(mean[1] > 0.8f * mean[0] && mean[1] < 1.2f * mean[0]);
  CHECK(fano[0] < 1.2f);
  CHECK(fano[1] > 2.5f * fano[0]);
  CHECK(gap[1] < 4.0f);
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
  TestWanderWalk();
  TestWanderKeepsPanSpread();
  TestOnlyTheirOwnRanges();
  TestShowersRate();
  return test_checker.failures() ? 1 : 0;
}

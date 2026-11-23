// Copyright 2012 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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

// Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/struck_bell_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kBellWavSineScale = 32638.0f / 32768.0f;
const float kBellWavSinePedestal = 127.0f / 32768.0f;

}  // namespace

void StruckBell::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void StruckBell::Reset() {
  for (size_t i = 0; i < kNumBellPartials; ++i) {
    amplitude_[i] = 0;
    phase_sin_[i] = 0.0f;
    phase_cos_[i] = 1.0f;
  }
  ever_struck_ = false;
}

void StruckBell::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = true;

  const bool unpatched = (parameters.trigger & TRIGGER_UNPATCHED) != 0;
  const bool rising_edge = (parameters.trigger & TRIGGER_RISING_EDGE) != 0;
  const bool self_strike = unpatched && !ever_struck_;
  const bool strike = rising_edge || self_strike;

  if (strike) {
    const float tilt = (parameters.morph - 0.5f) * 2.0f;
    const float half_span = (kNumBellPartials - 1) * 0.5f;
    for (size_t i = 0; i < kNumBellPartials; ++i) {
      const float normalized_index =
          (static_cast<float>(i) - half_span) / half_span;
      float gain = 1.0f + tilt * normalized_index * kBellBrightnessDepth;
      CONSTRAIN(gain, 0.0f, 3.0f);
      amplitude_[i] = static_cast<int32_t>(kBellPartialAmplitudes[i] * gain);
      phase_sin_[i] = 1.0f;
      phase_cos_[i] = 0.0f;
    }
    ever_struck_ = true;
  }

  const float timbre16 = floorf(parameters.timbre * 32767.0f);
  if (!unpatched && timbre16 < kBellDroneThreshold) {
    const float balance_raw = floorf((32767.0f - timbre16) / 256.0f);
    const float balance = floorf((balance_raw * balance_raw) / 128.0f);

    const float spread = kBellStockDecaySpread;

    int32_t block_decay[kNumBellPartials];
    int32_t decay_total = 0;
    for (size_t i = 0; i < kNumBellPartials; ++i) {
      const float decay_long = kBellPartialDecayLong[i];
      const float decay_short = kBellPartialDecayShort[i];
      const float decay16 = decay_long -
          floorf(((decay_long - decay_short) * balance) / 128.0f);
      block_decay[i] = static_cast<int32_t>(decay16);
      decay_total += block_decay[i];
    }
    const float mean_decay =
        static_cast<float>(decay_total) / static_cast<float>(kNumBellPartials);

    const size_t block_samples = static_cast<size_t>(kBellBlockSamples);
    size_t steps = (size + block_samples / 2) / block_samples;
    if (steps < 1) {
      steps = 1;
    }

    const float pull = 1.0f - spread;
    for (size_t i = 0; i < kNumBellPartials; ++i) {
      float adjusted = static_cast<float>(block_decay[i]) +
          (mean_decay - static_cast<float>(block_decay[i])) * pull;
      CONSTRAIN(adjusted, 0.0f, 65536.0f);
      const int32_t decay16 = static_cast<int32_t>(adjusted + 0.5f);
      for (size_t n = 0; n < steps; ++n) {
        amplitude_[i] = static_cast<int32_t>(
            (static_cast<int64_t>(amplitude_[i]) * decay16) >> 16);
      }
    }
  }

  const float color16 = floorf(parameters.harmonics * 32767.0f);
  const float detune_units = floorf(color16 / 128.0f);
  const float detune_semitones = detune_units / 128.0f;

  float increment_sin[kNumBellPartials];
  float increment_cos[kNumBellPartials];

  float partial_gain[kNumBellPartials];
  for (size_t i = 0; i < kNumBellPartials; ++i) {
    partial_gain[i] = static_cast<float>(amplitude_[i]) * (1.0f / 32768.0f);
    const float ratio_semitones = kBellPartials[i] / 128.0f;
    const float sign = (i & 1) ? 1.0f : -1.0f;
    const float base_note = parameters.note + ratio_semitones +
        sign * detune_semitones;
    const float increment = NoteToFrequency(base_note);

    increment_sin[i] = Sine(increment);
    increment_cos[i] = Sine(increment + 0.25f);
    const float rotation_norm = 1.0f / Sqrt(
        increment_sin[i] * increment_sin[i] +
        increment_cos[i] * increment_cos[i]);
    increment_sin[i] *= rotation_norm;
    increment_cos[i] *= rotation_norm;
  }

  for (size_t s = 0; s < size; ++s) {
    float sum_l = 0.0f;
    float upper = 0.0f;
    for (size_t i = 0; i < kNumBellPartials; ++i) {

      const float phase_sin = phase_sin_[i];
      const float phase_cos = phase_cos_[i];
      phase_sin_[i] = phase_sin * increment_cos[i] +
          phase_cos * increment_sin[i];
      phase_cos_[i] = phase_cos * increment_cos[i] -
          phase_sin * increment_sin[i];
      const float contribution =
          (-phase_cos_[i] * kBellWavSineScale + kBellWavSinePedestal) *
          partial_gain[i];
      sum_l += contribution;
      if (i >= kNumBellPartials - 5) {
        upper += contribution;
      }
    }

    float out_sample = sum_l * kBellOutputScale;
    CONSTRAIN(out_sample, -1.0f, 1.0f);
    *out++ = out_sample;

    float aux_sample = upper * kBellOutputScale;
    CONSTRAIN(aux_sample, -1.0f, 1.0f);
    *aux++ = aux_sample;
  }
}

}  // namespace plaits

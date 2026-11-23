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
//
//
// Harmonics: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/harmonics_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline int32_t ToBraidsParameter(float normalized) {
  int32_t value = static_cast<int32_t>(normalized * 32767.0f);
  CONSTRAIN(value, 0, 32767);
  return value;
}

}  // namespace

void Harmonics::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Harmonics::Reset() {
  phase_ = 0.0f;
  frequency_ = 0.01f;
  for (int i = 0; i < kHarmonicsNumPartials; ++i) {
    amplitude_[i] = 0;
    amplitude_aux_[i] = 0;
  }
}

void Harmonics::ComputeTargets(
    float peak_position,
    float colour,
    float spread,
    float width_scale,
    int32_t increment_msb,
    int32_t* target) {
  const int32_t p0 = ToBraidsParameter(peak_position);
  const int32_t p1 = ToBraidsParameter(colour);

  const int32_t peak = (kHarmonicsNumPartials * p0) >> 7;
  int32_t second_peak = (peak >> 1) +
      kHarmonicsNumPartials * (kHarmonicsPositionScale / 2) +
      static_cast<int32_t>(spread * static_cast<float>(kHarmonicsPositionScale));
  const int32_t second_peak_amount = p1 * p1 >> 15;

  const int32_t sqrtsqrt_width = p1 < 16384 ? p1 >> 6 : 511 - (p1 >> 6);
  const int32_t sqrt_width = sqrtsqrt_width * sqrtsqrt_width >> 10;
  int32_t width = sqrt_width * sqrt_width + 4;
  width = static_cast<int32_t>(static_cast<float>(width) * width_scale);
  if (width < 1) {
    width = 1;
  }

  int32_t total = 0;
  for (int i = 0; i < kHarmonicsNumPartials; ++i) {
    const int32_t x = i * kHarmonicsPositionScale;
    int32_t d = x - peak;
    int32_t g = 32768 * 128 / (128 + d * d / width);

    d = x - second_peak;
    g += second_peak_amount * 128 / (128 + d * d / width);
    total += g;
    target[i] = g;
  }

  const int32_t attenuation = 2147483647 / total;
  for (int i = 0; i < kHarmonicsNumPartials; ++i) {
    if (increment_msb * (i + 1) > kHarmonicsGuardThreshold) {
      target[i] = 0;
    } else {
      target[i] = target[i] * attenuation >> 16;
    }
  }
}

void Harmonics::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
  }

  const float frequency = NoteToFrequency(parameters.note);

  const int32_t increment_msb = static_cast<int32_t>(
      frequency * kHarmonicsIncrementScale);

  float peak_main = parameters.timbre;
  float peak_aux = 1.0f - parameters.timbre;

  const float spread = (parameters.morph - 0.5f) * 2.0f * kHarmonicsMaxSpread;
  const float width_scale = 1.0f;

  int32_t target[kHarmonicsNumPartials];
  int32_t target_aux[kHarmonicsNumPartials];
  ComputeTargets(
      peak_main, parameters.harmonics, spread, width_scale, increment_msb,
      &target[0]);
  ComputeTargets(
      peak_aux, parameters.harmonics, spread, width_scale, increment_msb,
      &target_aux[0]);

  ParameterInterpolator fm(&frequency_, frequency, size);
  const float* frequency_offset = NULL;

  while (size--) {
    float f = fm.Next();
    if (frequency_offset) {
      f += *frequency_offset++;
      CONSTRAIN(f, -0.5f, 0.499999f);
    }
    phase_ += f;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    } else if (phase_ < 0.0f) {
      phase_ += 1.0f;
    }

    float sum = 0.0f;
    float sum_aux = 0.0f;

    const float cosine_1 = Sine(phase_ + 0.25f);
    const float two_cosine_1 = 2.0f * cosine_1;
    float cosine_previous = 1.0f;
    float cosine = cosine_1;
    for (int i = 0; i < kHarmonicsNumPartials; ++i) {
      const float partial = -cosine;
      sum += partial * static_cast<float>(amplitude_[i]);
      sum_aux += partial * static_cast<float>(amplitude_aux_[i]);

      // Keep Braids' integer slew and its quiet-partial gate.
      amplitude_[i] += (target[i] - amplitude_[i]) >> 8;
      amplitude_aux_[i] += (target_aux[i] - amplitude_aux_[i]) >> 8;

      const float cosine_next =
          two_cosine_1 * cosine - cosine_previous;
      cosine_previous = cosine;
      cosine = cosine_next;
    }

    const float scale = kHarmonicsSineAmplitude / 32768.0f;
    float value = sum * scale;
    float value_aux = sum_aux * scale;
    CONSTRAIN(value, -1.0f, 1.0f);
    CONSTRAIN(value_aux, -1.0f, 1.0f);

    *out++ = value;
    *aux++ = value_aux;
  }
}

}  // namespace plaits

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
// PhaseFlock: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/phase_flock_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kFlockDetuning[kNumPhaseFlockOscillators] = {
  -1.0f, -0.63f, -0.31f, -0.07f, 0.19f, 0.51f, 0.86f
};

const float kFlockInitialPhase[kNumPhaseFlockOscillators] = {
  0.031f, 0.647f, 0.287f, 0.903f, 0.491f, 0.154f, 0.778f
};

const size_t kFlockCouplingInterval = 8;

inline float Wrap(float phase) {
  phase -= static_cast<int>(phase);
  if (phase < 0.0f) {
    phase += 1.0f;
  }
  return phase;
}

inline void SineCosineNoWrap(float phase, float* sine, float* cosine) {
  const float index = phase * kSineLUTSize;
  const int32_t integral = static_cast<int32_t>(index);
  const float fractional = index - static_cast<float>(integral);
  const float sine_a = lut_sine[integral];
  const float sine_b = lut_sine[integral + 1];
  const int32_t cosine_integral = integral + kSineLUTQuadrature;
  const float cosine_a = lut_sine[cosine_integral];
  const float cosine_b = lut_sine[cosine_integral + 1];
  *sine = sine_a + (sine_b - sine_a) * fractional;
  *cosine = cosine_a + (cosine_b - cosine_a) * fractional;
}

inline float LimitAudio(float sample) {
  // Limit synchronized flocks while preserving quiet phase cancellations.
  sample /= 1.0f + 0.30f * fabsf(sample);
  CONSTRAIN(sample, -1.0f, 1.0f);
  return sample;
}

}  // namespace

void PhaseFlock::Init(BufferAllocator* allocator) {
  Reset();
}

void PhaseFlock::Reset() {
  scatter_count_ = 0;
  reset_pending_ = true;
  for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
    sine_[i] = 0.0f;
    cosine_[i] = 1.0f;
  }
}

void PhaseFlock::Scatter() {
  ++scatter_count_;
  const float turn = 0.083f * static_cast<float>(scatter_count_ & 7);
  for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
    const float phase = Wrap(
        kFlockInitialPhase[i] + turn * static_cast<float>(i + 1));
    SineCosineNoWrap(phase, &sine_[i], &cosine_[i]);
  }
}

void PhaseFlock::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (reset_pending_ || (parameters.trigger & TRIGGER_RISING_EDGE)) {
    Scatter();
    reset_pending_ = false;
  }

  const float base_frequency = min(0.20f, NoteToFrequency(parameters.note));
  const float spread = 14.0f * parameters.harmonics *
      parameters.harmonics;
  float natural_frequency[kNumPhaseFlockOscillators];
  for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
    natural_frequency[i] = min(
        0.23f,
        base_frequency * SemitonesToRatio(spread * kFlockDetuning[i]));
  }

  const float coupling = min(
      0.045f,
      base_frequency * 1.8f * parameters.timbre * parameters.timbre);
  const float cluster_mix = parameters.morph;
  // The fixed macro adds a random phase kick at each coupling update.
  const float temperature = 0.5f <= 0.0f
      ? 0.0f
      : SemitonesToRatio(72.0f * (0.5f - 1.0f));
  float kick_scale = temperature * 2.68f *
      Sqrt(base_frequency) * 3.4641016f;  // sqrt(12): unit-variance uniform
  if (kick_scale > 0.9f) {
    kick_scale = 0.9f;
  }
  const float normalization = 1.0f /
      static_cast<float>(kNumPhaseFlockOscillators);

  float rotation_sine[kNumPhaseFlockOscillators];
  float rotation_cosine[kNumPhaseFlockOscillators];
  for (size_t sample = 0; sample < size; ++sample) {
    float sine[kNumPhaseFlockOscillators];
    float cosine[kNumPhaseFlockOscillators];
    float sine_2[kNumPhaseFlockOscillators];
    float cosine_2[kNumPhaseFlockOscillators];
    float mean_sine = 0.0f;
    float mean_cosine = 0.0f;
    float mean_sine_2 = 0.0f;
    float mean_cosine_2 = 0.0f;

    for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
      const float s = sine_[i];
      const float c = cosine_[i];
      sine[i] = s;
      cosine[i] = c;
      sine_2[i] = 2.0f * s * c;
      cosine_2[i] = c * c - s * s;
      mean_sine += s;
      mean_cosine += c;
      mean_sine_2 += sine_2[i];
      mean_cosine_2 += cosine_2[i];
    }

    mean_sine *= normalization;
    mean_cosine *= normalization;
    mean_sine_2 *= normalization;
    mean_cosine_2 *= normalization;

    out[sample] = LimitAudio(mean_sine * 1.55f);
    // The second circular moment preserves two-cluster states at AUX.
    aux[sample] = LimitAudio(mean_sine_2 * 1.35f);

    const bool update_coupling =
        (sample & (kFlockCouplingInterval - 1)) == 0;
    if (update_coupling) {
      for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
        // Blend attraction toward one flock or two antipodal clusters.
        const float attraction_1 = mean_sine * cosine[i] -
            mean_cosine * sine[i];
        const float attraction_2 = mean_sine_2 * cosine_2[i] -
            mean_cosine_2 * sine_2[i];
        const float attraction = attraction_1 +
            (0.5f * attraction_2 - attraction_1) * cluster_mix;
        const float correction = attraction;
        float increment = natural_frequency[i] + coupling * correction;
        CONSTRAIN(increment, 0.0f, 0.24f);
        const float rotation_phase = fabsf(increment);
        SineCosineNoWrap(
            rotation_phase, &rotation_sine[i], &rotation_cosine[i]);
        if (increment < 0.0f) {
          rotation_sine[i] = -rotation_sine[i];
        }
        const float rotation_norm = 1.5f - 0.5f * (
            rotation_sine[i] * rotation_sine[i] +
            rotation_cosine[i] * rotation_cosine[i]);
        rotation_sine[i] *= rotation_norm;
        rotation_cosine[i] *= rotation_norm;
        const float s = sine_[i];
        const float c = cosine_[i];
        sine_[i] = s * rotation_cosine[i] + c * rotation_sine[i];
        cosine_[i] = c * rotation_cosine[i] - s * rotation_sine[i];
        if (kick_scale > 0.0f) {
          const float kick = kick_scale * (Random::GetFloat() - 0.5f);
          float kick_sine;
          float kick_cosine;
          SineCosineNoWrap(fabsf(kick), &kick_sine, &kick_cosine);
          if (kick < 0.0f) {
            kick_sine = -kick_sine;
          }
          const float ks = sine_[i];
          const float kc = cosine_[i];
          sine_[i] = ks * kick_cosine + kc * kick_sine;
          cosine_[i] = kc * kick_cosine - ks * kick_sine;
        }
        if (sample == 0 || kick_scale > 0.0f) {
          const float state_norm = 1.5f - 0.5f * (
              sine_[i] * sine_[i] + cosine_[i] * cosine_[i]);
          sine_[i] *= state_norm;
          cosine_[i] *= state_norm;
        }
      }
    } else {
      for (int i = 0; i < kNumPhaseFlockOscillators; ++i) {
        const float s = sine_[i];
        const float c = cosine_[i];
        sine_[i] = s * rotation_cosine[i] + c * rotation_sine[i];
        cosine_[i] = c * rotation_cosine[i] - s * rotation_sine[i];
      }
    }
  }
}

}  // namespace plaits

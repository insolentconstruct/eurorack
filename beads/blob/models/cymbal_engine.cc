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
// Cymbal: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/cymbal_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"

namespace plaits {

using namespace stmlib;

namespace {

inline float ChamberlinBandpass(
    float* lp, float* bp, float in, float f, float damp) {
  const float notch = in - *bp * damp;
  *lp += f * (*bp);
  CONSTRAIN(*lp, -1.0f, 1.0f);
  const float hp = notch - *lp;
  *bp += f * hp;
  CONSTRAIN(*bp, -1.0f, 1.0f);
  return *bp;
}

inline float ChamberlinHighpass(
    float* lp, float* bp, float in, float f, float damp) {
  const float notch = in - *bp * damp;
  *lp += f * (*bp);
  CONSTRAIN(*lp, -1.0f, 1.0f);
  float hp = notch - *lp;
  *bp += f * hp;
  CONSTRAIN(*bp, -1.0f, 1.0f);
  CONSTRAIN(hp, -1.0f, 1.0f);
  return hp;
}

}  // namespace

void Cymbal::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Cymbal::Reset() {
  for (int p = 0; p < 6; ++p) {
    partial_phase_[p] = 0.0f;
  }
  bp_lp_ = 0.0f;
  bp_bp_ = 0.0f;
  for (int channel = 0; channel < 2; ++channel) {
    noise_clock_[channel] = 0.0f;
    hp_lp_[channel] = 0.0f;
    hp_bp_[channel] = 0.0f;
  }
  rng_state_[0] = 0u;
  rng_state_[1] = 0x9e3779b9u;
  xfade_ = 0.5f;
}

void Cymbal::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const int num_channels = 1;

  const float frequency0 = NoteToFrequency(
      kCymbalRootOffsetSemitones + parameters.note * 0.5f);

  const float morph_u = 2.0f * parameters.morph - 1.0f;
  const float spread = morph_cubic_
      ? kCymbalSpreadMax * (0.5f + 0.5f * morph_u * morph_u * morph_u)
      : parameters.morph * kCymbalSpreadMax;

  float increment[6];
  increment[0] = frequency0;
  increment[1] = frequency0 * (1.0f + (kCymbalPartialRatio1 - 1.0f) * spread);
  increment[2] = frequency0 * (1.0f + (kCymbalPartialRatio2 - 1.0f) * spread);
  increment[3] = frequency0 * (1.0f + (kCymbalPartialRatio3 - 1.0f) * spread);
  increment[4] = frequency0 * (1.0f + (kCymbalPartialRatio4 - 1.0f) * spread);
  increment[5] = frequency0 * (1.0f + (kCymbalPartialRatio5 - 1.0f) * spread);

  const float noise_clock_increment = frequency0 * kCymbalNoiseClockMultiplier;

  const float note_index = parameters.timbre * kCymbalToneIndexRange;
  float cutoff_hz = 440.0f * SemitonesToRatio(note_index - 69.0f);
  const float cutoff_cap_hz = kCymbalOversampledRate * kCymbalCutoffCapFraction;
  if (cutoff_hz > cutoff_cap_hz) {
    cutoff_hz = cutoff_cap_hz;
  }
  const float f_norm = cutoff_hz / kCymbalOversampledRate;
  const float f = 2.0f * sinf(float(M_PI) * f_norm);

  const float resonance_scale = 1.0f;
  const float damp_bp = kCymbalDampBandpassStock * resonance_scale;
  const float damp_hp = kCymbalDampHighpassStock * resonance_scale;

  ParameterInterpolator xfade_modulation(&xfade_, parameters.harmonics, size);

  float partial_phase[6];
  for (int p = 0; p < 6; ++p) {
    partial_phase[p] = partial_phase_[p];
  }
  float bp_lp = bp_lp_;
  float bp_bp = bp_bp_;

  float noise_clock[2];
  uint32_t rng_state[2];
  float hp_lp[2];
  float hp_bp[2];
  for (int channel = 0; channel < num_channels; ++channel) {
    noise_clock[channel] = noise_clock_[channel];
    rng_state[channel] = rng_state_[channel];
    hp_lp[channel] = hp_lp_[channel];
    hp_bp[channel] = hp_bp_[channel];
  }

  for (size_t i = 0; i < size; ++i) {
    float hat_noise_sum = 0.0f;
    for (int j = 0; j < 2; ++j) {
      for (int p = 0; p < 6; ++p) {
        float sample_increment = increment[p];
        partial_phase[p] += sample_increment * 0.5f;
        if (partial_phase[p] >= 1.0f) {
          partial_phase[p] -= 1.0f;
        }
      }

      int comparator_sum = 0;
      for (int p = 0; p < 6; ++p) {
        if (partial_phase[p] >= 0.5f) {
          ++comparator_sum;
        }
      }
      const float raw_metallic =
          static_cast<float>(comparator_sum - 3) * kCymbalMetallicScale;

      hat_noise_sum += ChamberlinBandpass(
          &bp_lp, &bp_bp, raw_metallic, f, damp_bp);
    }
    const float hat_noise = hat_noise_sum * 0.5f;

    const float xfade = xfade_modulation.Next();

    for (int channel = 0; channel < num_channels; ++channel) {
      float noise_filtered_sum = 0.0f;
      for (int j = 0; j < 2; ++j) {
        float sample_noise_clock_increment = noise_clock_increment;
        noise_clock[channel] += sample_noise_clock_increment * 0.5f;
        while (noise_clock[channel] >= 1.0f) {
          noise_clock[channel] -= 1.0f;
          rng_state[channel] = rng_state[channel] * 1664525u + 1013904223u;
        }
        const float raw_noise =
            (static_cast<float>((rng_state[channel] >> 16) & 0xffffu) -
             32768.0f) / 32768.0f;

        noise_filtered_sum += ChamberlinHighpass(
            &hp_lp[channel], &hp_bp[channel], raw_noise * 0.5f, f, damp_hp);
      }
      const float noise_filtered = noise_filtered_sum * 0.5f;

      out[i] = hat_noise + (noise_filtered - hat_noise) * xfade;
      aux[i] = hat_noise + (noise_filtered - hat_noise) * (1.0f - xfade);
    }
  }

  for (int p = 0; p < 6; ++p) {
    partial_phase_[p] = partial_phase[p];
  }
  bp_lp_ = bp_lp;
  bp_bp_ = bp_bp;
  for (int channel = 0; channel < num_channels; ++channel) {
    noise_clock_[channel] = noise_clock[channel];
    rng_state_[channel] = rng_state[channel];
    hp_lp_[channel] = hp_lp[channel];
    hp_bp_[channel] = hp_bp[channel];
  }
}

}  // namespace plaits

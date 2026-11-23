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
// ZFilter: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/z_filter_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {
using namespace std;
using namespace stmlib;

namespace {
// Braids uses a cosine origin, unlike the exported Plaits sine table.
inline float BraidsSine(float phase) {
  return Sine(phase + 0.75f);
}

// Preserve the gain, DC offset and quantization that drive the integrator.
inline int32_t BraidsSineFixed(float phase) {
  return static_cast<int32_t>(
      BraidsSine(phase) * 32639.0f + (32768.0f + 127.0f)) - 32768;
}

inline float BendWindow(float x, const float* weights) {
  const float x2 = x * x;
  const float x3 = x2 * x;
  const float x4 = x2 * x2;
  return x * weights[0] + x2 * weights[1] +
      x3 * weights[2] + x4 * weights[3];
}

inline float CombineModel(
    int filter_type,
    float carrier,
    float window,
    float pulse,
    float integrator,
    float balance) {
  float saw_tri_signal;
  float square_signal;
  if (filter_type & 2) {
    saw_tri_signal = carrier * window;
    square_signal = pulse;
  } else {
    saw_tri_signal = window * (carrier + 1.0f) - 1.0f;
    square_signal = filter_type == 1
        ? 0.5f * (pulse + integrator)
        : integrator;
  }
  return saw_tri_signal + (square_signal - saw_tri_signal) * balance;
}

}  // namespace

void ZFilter::Init(BufferAllocator* allocator) {
  (void) allocator;
  model_quantizer_.Init(4, 0.05f, false);
  Reset();
}

void ZFilter::Reset() {
  phase_ = 0.0f;
  mod_increment_ = 0.0f;
  previous_half_ = false;
  modulator_phase_ = 0.0f;
  square_phase_ = 0.0f;
  integrator_ = 0;

  polarity_ = false;
  out_decimator_ = 0.0f;
  aux_decimator_ = 0.0f;
}

void ZFilter::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    previous_half_ = false;
    modulator_phase_ = 0.0f;
    square_phase_ = 0.0f;
    integrator_ = 0;
    polarity_ = true;
  }

  const float base_f0 = NoteToFrequency(parameters.note) * 0.5f;

  float shifted_pitch = parameters.note + kZFilterCutoffOffset +
      kZFilterCutoffRange * parameters.timbre;
  if (shifted_pitch > kZFilterMaxPitch) {
    shifted_pitch = kZFilterMaxPitch;
  }
  const float target_mod_increment = NoteToFrequency(shifted_pitch) * 0.5f;

  const int filter_type = model_quantizer_.Process(parameters.harmonics);

  const int aux_filter_type = 3 - filter_type;

  const float balance = 1.0f - fabsf(2.0f * parameters.morph - 1.0f);
  const bool use_triangle = parameters.morph >= 0.5f;

  const float bend = 1.0f;
  const bool bend_active = bend > 1.0001f;
  float bend_weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
  if (bend_active) {
    const float k = bend - 1.0f;
    int index = static_cast<int>(k);
    CONSTRAIN(index, 0, 2);
    const float fraction = k - static_cast<float>(index);
    bend_weights[0] = 0.0f;
    bend_weights[index] = 1.0f - fraction;
    bend_weights[index + 1] = fraction;
  }

  const float modulator_reset = kZFilterPhaseReset[filter_type];
  const float square_reset = kZFilterPhaseReset[(filter_type & 1) + 2];

  ParameterInterpolator mod_increment_modulation(
      &mod_increment_, target_mod_increment, size * 2);

  for (size_t i = 0; i < size; ++i) {
    float f0 = base_f0;
    float out_sub[2];
    float aux_sub[2];
    for (int s = 0; s < 2; ++s) {
      float mod_increment = mod_increment_modulation.Next();

      phase_ += f0;

      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      }
      const bool half = phase_ >= 0.5f;

      const bool half_cycle = half != previous_half_;
      const bool cycle_start = half_cycle && !half;
      previous_half_ = half;

      modulator_phase_ += mod_increment;

      if (modulator_phase_ >= 1.0f) {
        modulator_phase_ -= 1.0f;
      }
      square_phase_ += mod_increment;

      if (square_phase_ >= 1.0f) {
        square_phase_ -= 1.0f;
      }
      if (cycle_start) {
        modulator_phase_ = modulator_reset;
      }
      if (half_cycle) {
        polarity_ = !polarity_;
        square_phase_ = square_reset;
      }

      const float saw = 1.0f - phase_;
      const float double_ramp = half
          ? 2.0f - 2.0f * phase_
          : 1.0f - 2.0f * phase_;
      const float triangle = half
          ? 2.0f - 2.0f * phase_
          : 2.0f * phase_;

      float window = use_triangle ? triangle : saw;
      float double_saw = double_ramp;
      if (bend_active) {
        window = BendWindow(window, bend_weights);
        double_saw = BendWindow(double_saw, bend_weights);
      }

      const float carrier =
          static_cast<float>(BraidsSineFixed(modulator_phase_)) *
          (1.0f / 32768.0f);

      // The pulse integrator stays in fixed point, as in Palette.
      const uint16_t double_saw_fixed =
          static_cast<uint16_t>(double_saw * 65535.0f);
      const uint16_t integrator_gain =
          static_cast<uint16_t>(mod_increment * 262144.0f);
      const int32_t square_carrier = BraidsSineFixed(square_phase_);

      int32_t pulse_fixed =
          (square_carrier * static_cast<int32_t>(double_saw_fixed)) >> 16;
      if (polarity_) {
        pulse_fixed = -pulse_fixed;
      }
      integrator_ += (pulse_fixed * static_cast<int32_t>(integrator_gain)) >> 16;
      CLIP(integrator_)

      const float pulse = static_cast<float>(pulse_fixed) * (1.0f / 32768.0f);
      const float integrator =
          static_cast<float>(integrator_) * (1.0f / 32768.0f);

      out_sub[s] = CombineModel(
          filter_type, carrier, window, pulse, integrator, balance);
      aux_sub[s] = CombineModel(
          aux_filter_type, carrier, window, pulse, integrator, balance);
    }

    // Decimate the two internal samples with the original three-tap filter.
    out[i] = 0.25f * out_decimator_ + 0.5f * out_sub[0] + 0.25f * out_sub[1];
    aux[i] = 0.25f * aux_decimator_ + 0.5f * aux_sub[0] + 0.25f * aux_sub[1];
    out_decimator_ = out_sub[1];
    aux_decimator_ = aux_sub[1];
  }
}

}  // namespace plaits

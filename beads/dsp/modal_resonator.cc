// Copyright 2015 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Rings' modal resonator, rendered by Plaits' batched SVFs.

#include "beads/dsp/modal_resonator.h"

#include <algorithm>

#include "stmlib/dsp/cosine_oscillator.h"
#include "stmlib/dsp/dsp.h"

#include "rings/resources.h"

namespace beads {

using namespace std;
using namespace stmlib;

void ModalResonator::Init(float sample_rate) {
  for (int i = 0; i < kNumResonatorModes / 8; ++i) {
    odd_[i].Init();
    even_[i].Init();
  }
  set_frequency(220.0f / sample_rate);
  set_structure(0.25f);
  set_brightness(0.5f);
  set_damping(0.5f);
  set_position(0.3f);
  limiter_.Init(
      1.0f / (sample_rate * 0.5f), kResonatorPeak * kResonatorHeadroom);
}

void ModalResonator::Render(const float* in, FloatFrame* frames, size_t size) {
  float l[size];
  float r[size];
  Process(in, l, r, size);
  FloatFrame resonance[size];
  for (size_t i = 0; i < size; ++i) {
    resonance[i].l = l[i] * kResonatorHeadroom;
    resonance[i].r = r[i] * kResonatorHeadroom;
  }
  limiter_.Process(resonance, size);
  for (size_t i = 0; i < size; ++i) {
    frames[i].l += resonance[i].l * (1.0f / kResonatorHeadroom);
    frames[i].r += resonance[i].r * (1.0f / kResonatorHeadroom);
  }
}

void ModalResonator::Process(
    const float* in,
    float* out,
    float* aux,
    size_t size) {
  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  // Rings' Resonator::ComputeFilters.
  float stiffness = Interpolate(rings::lut_stiffness, structure_, 256.0f);
  float harmonic = frequency_;
  float stretch_factor = 1.0f;
  float q = 500.0f * Interpolate(rings::lut_4_decades, damping_, 256.0f);
  float brightness_attenuation = 1.0f - structure_;
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
  float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
  float q_loss_damping_rate = structure_ * (2.0f - structure_) * 0.1f;

  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position_);

  float mode_f[2][plaits::kModeBatchSize];
  float mode_q[2][plaits::kModeBatchSize];
  float mode_a[2][plaits::kModeBatchSize];
  plaits::ResonatorSvf<plaits::kModeBatchSize>* odd = &odd_[0];
  plaits::ResonatorSvf<plaits::kModeBatchSize>* even = &even_[0];
  for (int batch = 0; batch < kNumResonatorModes / 8; ++batch) {
    bool audible = false;
    for (int i = 0; i < 8; ++i) {
      float partial_frequency = harmonic * stretch_factor;
      // Rings' input gain of 0.125 is folded into the mode amplitude.
      float amplitude = amplitudes.Next() * 0.125f;
      if (partial_frequency >= 0.49f) {
        partial_frequency = 0.49f;
        amplitude = 0.0f;
      } else {
        audible = true;
      }
      mode_f[i & 1][i >> 1] = partial_frequency;
      mode_q[i & 1][i >> 1] = 1.0f + partial_frequency * q;
      mode_a[i & 1][i >> 1] = amplitude;
      stretch_factor += stiffness;
      if (stiffness < 0.0f) {
        stiffness *= 0.93f;
      } else {
        stiffness *= 0.98f;
      }
      q_loss += q_loss_damping_rate * (1.0f - q_loss);
      harmonic += frequency_;
      q *= q_loss;
    }
    if (audible) {
      odd->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[0], mode_q[0], mode_a[0], in, out, size);
      even->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[1], mode_q[1], mode_a[1], in, aux, size);
    }
    ++odd;
    ++even;
  }
}

}  // namespace beads

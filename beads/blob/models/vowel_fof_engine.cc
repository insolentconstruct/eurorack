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
// VowelFof: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/vowel_fof_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "beads/blob/models/vowel_fof_data.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {
inline float InterpolateFormant(
    const int32_t* table, float voice_register, float vowel, int formant) {
  const float x = voice_register * static_cast<float>(kVowelFofGridSize - 1);
  const float y = vowel * static_cast<float>(kVowelFofGridSize - 1);
  int xi = static_cast<int>(x);
  int yi = static_cast<int>(y);
  CONSTRAIN(xi, 0, kVowelFofGridSize - 2);
  CONSTRAIN(yi, 0, kVowelFofGridSize - 2);
  const float xf = x - static_cast<float>(xi);
  const float yf = y - static_cast<float>(yi);
  const int stride = kVowelFofGridSize * kVowelFofNumFormants;
  const float a = static_cast<float>(
      table[xi * stride + yi * kVowelFofNumFormants + formant]);
  const float b = static_cast<float>(
      table[(xi + 1) * stride + yi * kVowelFofNumFormants + formant]);
  const float c = static_cast<float>(
      table[xi * stride + (yi + 1) * kVowelFofNumFormants + formant]);
  const float d = static_cast<float>(
      table[(xi + 1) * stride + (yi + 1) * kVowelFofNumFormants + formant]);
  const float ab = a + (b - a) * xf;
  const float cd = c + (d - c) * xf;
  return ab + (cd - ab) * yf;
}
inline float RenderFormant(
    int k,
    float saw,
    float noise,
    float noise_makeup,
    float breath,
    float f,
    float* svf_lp,
    float* svf_bp) {
  const float in = saw + (noise * noise_makeup - saw) * breath;
  const float notch = in - svf_bp[k] * kVowelFofDamping;
  svf_lp[k] += f * svf_bp[k];
  CONSTRAIN(svf_lp[k], -1.0f, 1.0f);
  const float hp = notch - svf_lp[k];
  svf_bp[k] += f * hp;
  CONSTRAIN(svf_bp[k], -1.0f, 1.0f);
  return svf_bp[k];
}

}  // namespace

void VowelFof::Init(BufferAllocator* allocator) {
  (void) allocator;
  excitation_.Init();
  Reset();
}

void VowelFof::Reset() {
  for (int i = 0; i < kVowelFofNumFormants; ++i) {
    svf_lp_[i] = 0.0f;
    svf_bp_[i] = 0.0f;
  }
  noise_state_ = 0x21f2ac31;
}

void VowelFof::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Reset();
  }

  const float frequency = NoteToFrequency(parameters.note);
  const float tilt = 0.0f;

  const bool stereo = false;

  float f[kVowelFofNumFormants];
  float raw_amplitude[kVowelFofNumFormants];
  float amplitude[kVowelFofNumFormants];
  float amplitude_aux[kVowelFofNumFormants];
  float noise_makeup[kVowelFofNumFormants];

  for (int i = 0; i < kVowelFofNumFormants; ++i) {
    const float note = InterpolateFormant(
        kVowelFofFrequency, parameters.morph, parameters.timbre, i) *
        (1.0f / 128.0f);
    const float formant_frequency = NoteToFrequency(note);
    float coefficient = 2.0f * Sine(0.5f * formant_frequency);
    CONSTRAIN(coefficient, 0.0f, 1.0f);
    f[i] = coefficient;

    const float raw = InterpolateFormant(
        kVowelFofAmplitude, parameters.morph, parameters.timbre, i) *
        (1.0f / 16384.0f);
    raw_amplitude[i] = raw;
    amplitude[i] = 1.0f + (raw - 1.0f) * tilt;
    noise_makeup[i] = Sqrt(0.02f / max(coefficient, 1e-4f));
  }

  const float breath = parameters.harmonics;
  if (stereo) {
    for (int i = 0; i < kVowelFofNumFormants; ++i) {
      amplitude_aux[i] = kVowelFofStereoMakeup *
          raw_amplitude[kVowelFofNumFormants - 1 - i];
    }
  }
  float source_makeup = 0.0f;
  if (!stereo) {
    for (int i = 0; i < kVowelFofNumFormants; ++i) {
      source_makeup += noise_makeup[i];
    }
    source_makeup *= 1.0f / static_cast<float>(kVowelFofNumFormants);
  }
  float excitation[kMaxBlockSize];
  excitation_.Render<OSCILLATOR_SHAPE_SAW>(
      frequency, 0.0f, excitation, size);
  if (stereo) {
    for (size_t i = 0; i < size; ++i) {
      const float saw = kVowelFofExcitationScale * (excitation[i] + 1.0f);

      noise_state_ = noise_state_ * 1664525u + 1013904223u;
      const float noise = static_cast<float>(noise_state_ >> 9) *
          (1.0f / 8388608.0f) - 1.0f;

      float sum = 0.0f;
      float sum_aux = 0.0f;
      for (int k = 0; k < kVowelFofNumFormants; ++k) {
        const float tap = RenderFormant(
            k, saw, noise, noise_makeup[k], breath, f[k], svf_lp_, svf_bp_);
        sum += tap * amplitude[k];
        sum_aux += tap * amplitude_aux[k];
      }

      out[i] = kVowelFofOutputScale * sum;
      aux[i] = kVowelFofOutputScale * sum_aux;
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      const float source = excitation[i] * kVowelFofExcitationScale;
      const float saw = source + kVowelFofExcitationScale;

      noise_state_ = noise_state_ * 1664525u + 1013904223u;
      const float noise = static_cast<float>(noise_state_ >> 9) *
          (1.0f / 8388608.0f) - 1.0f;

      float sum = 0.0f;
      for (int k = 0; k < kVowelFofNumFormants; ++k) {
        const float tap = RenderFormant(
            k, saw, noise, noise_makeup[k], breath, f[k], svf_lp_, svf_bp_);
        sum += tap * amplitude[k];
      }

      out[i] = kVowelFofOutputScale * sum;
      aux[i] = kVowelFofSourceGain *
          (source + (noise * source_makeup - source) * breath);
    }
  }
}

}  // namespace plaits

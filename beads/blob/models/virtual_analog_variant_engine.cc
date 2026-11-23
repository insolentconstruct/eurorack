// Copyright 2016 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
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
// Palette port: mono, no FM input, fourth macro fixed at 0.5.

#include "beads/blob/models/virtual_analog_variant_engine.h"

#include <algorithm>

#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kIntervals[5] = {
  0.0f, 7.01f, 12.01f, 19.01f, 24.01f
};

inline float Squash(float x) {
  return x * x * (3.0f - 2.0f * x);
}

inline void ShapeAndPulseWidth(float control, float* shape, float* pw) {
  float s = control * 1.5f;
  CONSTRAIN(s, 0.0f, 1.0f);
  *shape = s;
  float p = 0.5f + (control - 0.66f) * 1.4f;
  CONSTRAIN(p, 0.5f, 0.99f);
  *pw = p;
}

}  // namespace

void VirtualAnalogVariant::Init(BufferAllocator* allocator) {
  primary_.Init();
  auxiliary_.Init();
  auxiliary_.set_master_phase(0.25f);
  sync_.Init();
  aux_sync_.Init();

  auxiliary_amount_ = 0.0f;
  xmod_amount_ = 0.0f;
  temp_buffer_ = allocator->Allocate<float>(kMaxBlockSize);
  secondary_buffer_ = allocator->Allocate<float>(kMaxBlockSize);
}

void VirtualAnalogVariant::Reset() {
}

float VirtualAnalogVariant::ComputeDetuning(float detune) const {
  detune = 2.05f * detune - 1.025f;
  CONSTRAIN(detune, -1.0f, 1.0f);

  float sign = detune < 0.0f ? -1.0f : 1.0f;
  detune = detune * sign * 3.9999f;
  MAKE_INTEGRAL_FRACTIONAL(detune);

  const float a = kIntervals[detune_integral];
  const float b = kIntervals[detune_integral + 1];
  return (a + (b - a) * Squash(Squash(detune_fractional))) * sign;
}

void VirtualAnalogVariant::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  (void) already_enveloped;

#if PLAITS_VA_VARIANT_TRAJECTORY_ON_TWIST
  const float trajectory = 0.5f;
  const float secondary_control = parameters.morph;
#else
  const float trajectory = parameters.morph;
  const float secondary_control = parameters.timbre;
#endif

  float auxiliary_amount;
  float xmod_amount;
  if (remedy_ == VARIANT_REMEDY_NO_DRY) {
    auxiliary_amount = 0.5f;
    xmod_amount = trajectory;
  } else {
    auxiliary_amount = max(0.5f - trajectory, 0.0f) * 2.0f;
    if (remedy_ == VARIANT_REMEDY_LINEAR_FADE) {
      auxiliary_amount *= 0.5f;
    } else {
      auxiliary_amount *= auxiliary_amount * 0.5f;
    }
    xmod_amount = max(trajectory - 0.5f, 0.0f) * 2.0f;
  }
  const float squashed_xmod_amount = xmod_amount * (2.0f - xmod_amount);

  const float auxiliary_detune = ComputeDetuning(parameters.harmonics);
  const float primary_f = NoteToFrequency(parameters.note);
  const float auxiliary_f = NoteToFrequency(
      parameters.note + auxiliary_detune);
  const float sync_f = primary_f * SemitonesToRatio(
      xmod_amount * (auxiliary_detune + 36.0f));
  const float aux_sync_f = NoteToFrequency(
      parameters.note + parameters.harmonics * 48.0f);

  const float primary_control = parameters.timbre;
  float primary_shape, primary_pw;
  ShapeAndPulseWidth(primary_control, &primary_shape, &primary_pw);
  float secondary_shape, secondary_pw;
  ShapeAndPulseWidth(secondary_control, &secondary_shape, &secondary_pw);

  primary_.Render(
      primary_f, primary_pw, primary_shape, out, size);

  if (remedy_ == VARIANT_REMEDY_NO_DRY) {
    auxiliary_.Render(
        auxiliary_f, secondary_pw, secondary_shape, temp_buffer_, size);
    aux_sync_.Render(
        primary_f, aux_sync_f, secondary_pw, secondary_shape,
        secondary_buffer_, size);
    ParameterInterpolator blend(&xmod_amount_, trajectory, size);
    for (size_t i = 0; i < size; ++i) {
      const float primary = out[i];
      const float t = blend.Next();
      const float secondary =
          temp_buffer_[i] + (secondary_buffer_[i] - temp_buffer_[i]) * t;
      out[i] = (primary + secondary) * 0.5f;
      aux[i] = (secondary_buffer_[i] + primary) * 0.5f;
    }
    return;
  }

  ParameterInterpolator xmod_amount_modulation(
      &xmod_amount_,
      squashed_xmod_amount * (2.0f - squashed_xmod_amount),
      size);
  if (xmod_amount > 0.0f) {
    sync_.Render(
        primary_f, sync_f, secondary_pw, secondary_shape, temp_buffer_, size);
    for (size_t i = 0; i < size; ++i) {
      out[i] += (temp_buffer_[i] - out[i]) * xmod_amount_modulation.Next();
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      xmod_amount_modulation.Next();
    }
  }

  ParameterInterpolator auxiliary_amount_modulation(
      &auxiliary_amount_, auxiliary_amount, size);
  const bool need_auxiliary = auxiliary_amount > 0.0f || auxiliary_amount_ > 0.0f;
  if (need_auxiliary) {
    auxiliary_.Render(
        auxiliary_f, secondary_pw, secondary_shape, temp_buffer_, size);
  }

  aux_sync_.Render(
      primary_f, aux_sync_f, secondary_pw, secondary_shape, aux, size);

  if (need_auxiliary) {
    for (size_t i = 0; i < size; ++i) {
      const float primary = out[i];
      out[i] = primary +
          (temp_buffer_[i] - primary) * auxiliary_amount_modulation.Next();
      aux[i] = (aux[i] + primary) * 0.5f;
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      auxiliary_amount_modulation.Next();
      aux[i] = (aux[i] + out[i]) * 0.5f;
    }
  }
}

}  // namespace plaits

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
// CSaw: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/csaw_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

namespace plaits {
using namespace std;
using namespace stmlib;

namespace {
inline float BendSegment(float u, float bend) {
  return u * (1.0f - bend) + bend * u * u;
}

inline float BendSegmentSlope(float u, float bend) {
  return (1.0f - bend) + 2.0f * bend * u;
}

}  // namespace

void CSaw::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void CSaw::Reset() {
  phase_ = 0.0f;
  frequency_ = 0.01f;
  pw_ = 0.25f;
  bend_ = 0.0f;
  tilt_ = 0.0f;
  previous_pw_ = 0.25f;
  high_ = false;
  depth_ = 0.0f;
  next_sample_ = 0.0f;
  next_sample_aux_ = 0.0f;
}

void CSaw::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    high_ = false;
    next_sample_ = 0.0f;
    next_sample_aux_ = 0.0f;
  }

  const float frequency = NoteToFrequency(parameters.note);

  const float target_depth = kCSawDepthOffset +
      kCSawDepthRange * parameters.harmonics;

  const float dc = kCSawDcShift * (1.0f - parameters.harmonics);

  float pw_min = 8.0f * frequency;
  if (pw_min > 0.3f) {
    pw_min = 0.3f;
  }
  float target_pw = parameters.timbre * kCSawMaxPulseWidth;
  CONSTRAIN(target_pw, pw_min, kCSawMaxPulseWidth);

  const float target_bend = 2.0f * parameters.morph - 1.0f;
  const float target_tilt = 0.0f;

  ParameterInterpolator fm(&frequency_, frequency, size);
  ParameterInterpolator pwm(&pw_, target_pw, size);
  ParameterInterpolator bend_modulation(&bend_, target_bend, size);
  ParameterInterpolator tilt_modulation(&tilt_, target_tilt, size);

  float next_sample = next_sample_;
  float next_sample_aux = next_sample_aux_;

  for (size_t i = 0; i < size; ++i) {
    float this_sample = next_sample;
    float this_sample_aux = next_sample_aux;
    next_sample = 0.0f;
    next_sample_aux = 0.0f;

    float f = fm.Next();
    const float pw = pwm.Next();
    const float bend = bend_modulation.Next();
    const float tilt = tilt_modulation.Next();

    const float plateau_slope = tilt * (pw - depth_) / pw;
    const float slope_in = BendSegmentSlope(0.0f, bend);
    const float slope_out = BendSegmentSlope(1.0f, bend);

    phase_ += f;

    if (!high_ && phase_ >= pw) {
      float t = (phase_ - pw) / (previous_pw_ - pw + f);
      CONSTRAIN(t, 0.0f, 1.0f);
      const float this_blep = ThisBlepSample(t);
      const float next_blep = NextBlepSample(t);
      const float this_int = ThisIntegratedBlepSample(t);
      const float next_int = NextIntegratedBlepSample(t);

      const float step = (pw - depth_) * (1.0f - tilt);
      this_sample += step * this_blep;
      next_sample += step * next_blep;

      const float slope_step = (slope_in - plateau_slope) * f;
      this_sample += slope_step * this_int;
      next_sample += slope_step * next_int;

      this_sample_aux += kCSawPulseStep * this_blep;
      next_sample_aux += kCSawPulseStep * next_blep;

      high_ = true;
    } else if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
      float t = phase_ / f;
      CONSTRAIN(t, 0.0f, 1.0f);
      const float this_blep = ThisBlepSample(t);
      const float next_blep = NextBlepSample(t);
      const float this_int = ThisIntegratedBlepSample(t);
      const float next_int = NextIntegratedBlepSample(t);

      depth_ = target_depth;

      const float step = depth_ - 1.0f;
      this_sample += step * this_blep;
      next_sample += step * next_blep;

      const float new_plateau_slope = tilt * (pw - depth_) / pw;
      const float slope_step = (new_plateau_slope - slope_out) * f;
      this_sample += slope_step * this_int;
      next_sample += slope_step * next_int;

      this_sample_aux -= kCSawPulseStep * this_blep;
      next_sample_aux -= kCSawPulseStep * next_blep;

      high_ = false;
    }

    float naive;
    float naive_aux;
    if (phase_ < pw) {
      const float ramp = phase_ / pw;
      naive = depth_ + tilt * (pw - depth_) * ramp;
      naive_aux = -1.0f;
    } else {
      const float u = (phase_ - pw) / (1.0f - pw);
      naive = pw + (1.0f - pw) * BendSegment(u, bend);

      naive_aux = 1.0f;
    }
    next_sample += naive;
    next_sample_aux += naive_aux;
    previous_pw_ = pw;

    out[i] = kCSawMakeUp * (this_sample - 0.5f + dc);
    aux[i] = kCSawPulseGain * (this_sample_aux - (1.0f - 2.0f * pw));
  }

  next_sample_ = next_sample;
  next_sample_aux_ = next_sample_aux;
}

}  // namespace plaits

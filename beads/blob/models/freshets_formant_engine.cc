// Copyright 2026 Dylan Bolink.
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
// FreshetsFormant: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/freshets_formant_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/units.h"

#include "plaits/resources.h"

namespace plaits {

using namespace stmlib;

namespace {

const float kFormantShapeSize = 128.0f;

const int16_t kFormantShapeSin[] = {
  0,5,20,44,79,123,177,241,315,398,491,
  593,705,827,958,1098,1247,1406,1573,1749,1935,2128,
  2331,2542,2761,2989,3224,3468,3719,3978,4244,4518,4799,
  5086,5381,5682,5990,6304,6624,6950,7281,7618,7961,8308,
  8660,9017,9379,9744,10114,10487,10864,11244,11628,12014,12403,
  12794,13187,13583,13980,14378,14778,15178,15580,15981,16383,16786,
  17187,17589,17989,18389,18787,19184,19580,19973,20364,20753,21139,
  21523,21903,22280,22653,23023,23388,23750,24107,24459,24806,25149,
  25486,25817,26143,26463,26777,27085,27386,27681,27968,28249,28523,
  28789,29048,29299,29543,29778,30006,30225,30436,30639,30832,31018,
  31194,31361,31520,31669,31809,31940,32062,32174,32276,32369,32452,
  32526,32590,32644,32688,32723,32747,32762,32767,
};

// Select the exact service-table offsets without an absolute pointer table.
const int16_t* FormantShape(int index) {
  switch (index) {
    case 0:
      return lut_ws_inverse_tan + 128;
    case 1:
      return lut_ws_inverse_sin + 128;
    case 2:
      return lut_ws_linear + 128;
    case 3:
      return kFormantShapeSin;
    default:
      return lut_ws_bump + 128;
  }
}

inline float CenterDetent(float x) {
  if (x < 0.49f) {
    x *= 1.02040816f;
  } else if (x > 0.51f) {
    x -= 0.02f;
    x *= 1.02040816f;
  } else {
    x = 0.5f;
  }
  return x;
}

inline void RetriggerSerge(float* env_phase, float pw) {
  if (*env_phase >= pw) {
    float current_level = 0.0f;
    if (*env_phase < 1.0f) {
      current_level = 1.0f - (*env_phase - pw) / (1.0f - pw);
    }
    *env_phase = current_level * pw;
  }
}

const float kOutputScale = 5.0f / 10.0f;

const float kMaxShapeIndex = kFormantShapeSize - 0.0001f;

const float kStereoDetune = 0.15f;

}  // namespace

void FreshetsFormant::Init(stmlib::BufferAllocator* allocator) {
  dc_blocker_[0].Init();
  dc_blocker_[1].Init();
  Reset();
}

void FreshetsFormant::Reset() {
  driver_phase_ = 0.0f;
  env_phase_ = 1.0f;
  env_phase_r_ = 1.0f;
  prev_f0_ = 0.001f;
  prev_ratio_ = 1.0f;
  prev_pw_ = 0.5f;
  prev_shape_ = 2.0f;
  next_sample_hollow_ = 0.0f;
  next_sample_sub_ = 0.0f;
  sub_state_ = false;
  prev_hollow_state_ = 0;
  lp_1_ = 0.0f;
  lp_2_ = 0.0f;
  lp_1_r_ = 0.0f;
  lp_2_r_ = 0.0f;
}

void FreshetsFormant::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const float target_ratio = SemitonesToRatio(
      (CenterDetent(parameters.harmonics) - 0.5f) * 96.0f);

  const float smoothness = parameters.timbre;
  const float fold = std::max(2.0f * (smoothness - 0.5f), 0.0f);

  float pw = parameters.morph;
  CONSTRAIN(pw, 0.05f, 0.95f);

  const float shape = 2.0f;

  float f0 = NoteToFrequency(parameters.note);
  CONSTRAIN(f0, 0.0f, 0.25f);

  const bool sync_driver = parameters.trigger & TRIGGER_RISING_EDGE;
  if (sync_driver) {
    sub_state_ = false;
    RetriggerSerge(&env_phase_, pw);
    RetriggerSerge(&env_phase_r_, pw);
  }

  ParameterInterpolator f0_mod(&prev_f0_, f0, size);
  ParameterInterpolator ratio_mod(&prev_ratio_, target_ratio, size);
  ParameterInterpolator pw_mod(&prev_pw_, pw, size);
  ParameterInterpolator shape_mod(&prev_shape_, shape, size);

  float lp_ratio = std::min(smoothness * 2.0f, 1.0f);
  lp_ratio *= lp_ratio * lp_ratio;
  float base_lp_coefficient = f0 * 0.5f;
  base_lp_coefficient += (1.0f - base_lp_coefficient) * lp_ratio;

  for (size_t i = 0; i < size; ++i) {
    float f0 = f0_mod.Next();
    const float ratio = ratio_mod.Next();
    const float pw = pw_mod.Next();
    const float shape = shape_mod.Next();
    const float formant_freq = f0 * ratio;
    float lp_coefficient = base_lp_coefficient;

    const bool prev_sub = sub_state_;
    driver_phase_ += f0;
    if (i == 0 && sync_driver) {
      driver_phase_ = 0.0f;
    }
    if (driver_phase_ >= 1.0f) {
      driver_phase_ -= static_cast<int>(driver_phase_);
      sub_state_ = !sub_state_;
      RetriggerSerge(&env_phase_, pw);
      RetriggerSerge(&env_phase_r_, pw);
    }

    env_phase_ += formant_freq;
    if (env_phase_ > 1.0f) {
      env_phase_ = 1.0f;
    }

    env_phase_r_ += formant_freq * (1.0f + kStereoDetune);
    if (env_phase_r_ > 1.0f) {
      env_phase_r_ = 1.0f;
    }

    float raw_level;
    if (env_phase_ < pw) {
      raw_level = env_phase_ / pw;
    } else if (env_phase_ < 1.0f) {
      raw_level = 1.0f - (env_phase_ - pw) / (1.0f - pw);
    } else {
      raw_level = 0.0f;
    }

    MAKE_INTEGRAL_FRACTIONAL(shape);
    const int16_t* shape_1 = FormantShape(shape_integral);
    const int16_t* shape_2 = FormantShape(shape_integral + 1);

    float ws_index = kFormantShapeSize * raw_level;
    if (ws_index > kMaxShapeIndex) {
      ws_index = kMaxShapeIndex;
    }
    MAKE_INTEGRAL_FRACTIONAL(ws_index);

    const float x0 = static_cast<float>(shape_1[ws_index_integral]) / 32768.0f;
    const float x1 =
        static_cast<float>(shape_1[ws_index_integral + 1]) / 32768.0f;
    const float y0 = static_cast<float>(shape_2[ws_index_integral]) / 32768.0f;
    const float y1 =
        static_cast<float>(shape_2[ws_index_integral + 1]) / 32768.0f;
    const float x = x0 + (x1 - x0) * ws_index_fractional;
    const float y = y0 + (y1 - y0) * ws_index_fractional;
    const float shaped = x + (y - x) * shape_fractional;

    float bipolar = 2.0f * shaped - 1.0f;
    if (fold > 0.0f) {
      float index = 0.5f + bipolar * (0.03f + 0.46f * fold);
      CONSTRAIN(index, 0.0f, 1.0f);
      const float folded = InterpolateHermite(lut_fold + 1, index, 512.0f);
      bipolar = bipolar + (folded - bipolar) * fold;
    }
    ONE_POLE(lp_1_, bipolar, lp_coefficient);
    ONE_POLE(lp_2_, lp_1_, lp_coefficient);

    float formant = lp_2_ * kOutputScale;
    CONSTRAIN(formant, -1.0f, 1.0f);

    float hollow = 0.0f;
    int hollow_state;
    if (env_phase_ >= 1.0f || raw_level < 0.001f) {
      hollow_state = 0;
    } else if (env_phase_ < pw) {
      hollow_state = 1;
    } else {
      hollow_state = -1;
    }

    float this_sample = next_sample_hollow_;
    float next_sample = 0.0f;
    if (hollow_state != prev_hollow_state_) {
      const float discontinuity =
          static_cast<float>(hollow_state - prev_hollow_state_);
      this_sample += ThisBlepSample(0.0f) * discontinuity * 0.5f;
      next_sample += NextBlepSample(0.0f) * discontinuity * 0.5f;
    }
    next_sample += static_cast<float>(hollow_state);
    next_sample_hollow_ = next_sample;
    prev_hollow_state_ = hollow_state;

    float sub_sample = next_sample_sub_;
    float sub_next = 0.0f;
    if (sub_state_ != prev_sub) {
      const float discontinuity = sub_state_ ? 1.0f : -1.0f;
      sub_sample += ThisBlepSample(0.0f) * discontinuity;
      sub_next += NextBlepSample(0.0f) * discontinuity;
    }
    sub_next += sub_state_ ? 1.0f : 0.0f;
    next_sample_sub_ = sub_next;

    hollow = this_sample;
    if (smoothness < 0.5f) {
      hollow *= smoothness * 2.0f;
    } else if (smoothness > 0.5f) {
      const float sub_bipolar = 2.0f * sub_sample - 1.0f;
      hollow += sub_bipolar * (smoothness - 0.5f) * 2.0f;
    }
    hollow *= kOutputScale;
    CONSTRAIN(hollow, -1.0f, 1.0f);

    out[i] = formant;
    aux[i] = hollow;
  }

  dc_blocker_[0].set_f<FREQUENCY_DIRTY>(0.3f * f0);
  dc_blocker_[0].Process<FILTER_MODE_HIGH_PASS>(out, size);
  dc_blocker_[1].set_f<FREQUENCY_DIRTY>(0.15f * f0);
  dc_blocker_[1].Process<FILTER_MODE_HIGH_PASS>(aux, size);
}

}  // namespace plaits

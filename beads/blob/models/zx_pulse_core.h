// Copyright 2026 Combust.
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
// Shared pin channel and frame clock for the ZX models.

#ifndef BEADS_BLOB_MODELS_ZX_PULSE_CORE_H_
#define BEADS_BLOB_MODELS_ZX_PULSE_CORE_H_

#include "plaits/dsp/dsp.h"

namespace plaits {
namespace pulse {

inline float PolyBlep(float t, float dt) {
  if (t < dt) {
    const float x = t / dt;
    return x + x - x * x - 1.0f;
  } else if (t > 1.0f - dt) {
    const float x = (t - 1.0f) / dt;
    return x * x + x + x + 1.0f;
  }
  return 0.0f;
}

class PinChannel {
 public:
  void Init() { phase_ = 0.0f; }
  void Reset() { phase_ = 0.0f; }

  inline float Next(float dt, float duty, float* naive_out) {
    phase_ += dt;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    const float t = phase_;
    const float naive = t < duty ? 1.0f - duty : -duty;
    float tf = t - duty;
    if (tf < 0.0f) {
      tf += 1.0f;
    }
    *naive_out = naive;
    return naive + PolyBlep(t, dt) - PolyBlep(tf, dt);
  }

  inline float phase() const { return phase_; }

 private:
  float phase_;
};

class FrameTick {
 public:
  void Init(int32_t period_samples) {
    period_ = period_samples;
    counter_ = 1;
  }
  void Reset() { counter_ = 1; }

  inline bool Next() {
    if (--counter_ <= 0) {
      counter_ = period_;
      return true;
    }
    return false;
  }

 private:
  int32_t period_;
  int32_t counter_;
};

const int32_t kFrameTickPeriod = 957;

}  // namespace pulse
}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_ZX_PULSE_CORE_H_

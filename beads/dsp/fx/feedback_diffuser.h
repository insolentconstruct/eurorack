// Copyright 2026 OpenAI.
//
// Author: OpenAI
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
// Experimental wet feedback, independent of the frozen recording buffer.

#ifndef BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_
#define BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_

#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/dsp/fx/diffuser.h"

namespace beads {

class FeedbackDiffuser {
 public:
  FeedbackDiffuser() { }
  ~FeedbackDiffuser() { }

  void Init() {
    diffuser_.Init(buffer_);
    diffuser_.set_amount(1.0f);
    previous_.l = previous_.r = 0.0f;
    amount_ = 0.0f;
  }

  void Process(FloatFrame* in_out, float feedback, size_t size) {
    CONSTRAIN(feedback, 0.0f, 1.0f);
    stmlib::ParameterInterpolator amount(&amount_, feedback, size);
    while (size--) {
      const float blend = amount.Next();
      const float regeneration = 0.85f * blend;
      FloatFrame wet = *in_out * (1.0f - regeneration)
          + previous_ * regeneration;
      diffuser_.Process(&wet, 1);
      previous_ = wet;
      *in_out += (wet - *in_out) * blend;
      ++in_out;
    }
  }

 private:
  Diffuser diffuser_;
  uint16_t buffer_[2048];
  FloatFrame previous_;
  float amount_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackDiffuser);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_

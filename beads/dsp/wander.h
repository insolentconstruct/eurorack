// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Random values with memory: each grain's random offset steps from the
// previous grain's instead of being drawn afresh.

#ifndef BEADS_DSP_WANDER_H_
#define BEADS_DSP_WANDER_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/granular_player.h"
#include "beads/dsp/pitch_quantizer.h"
#include "beads/dsp/randomized_parameter.h"

namespace beads {

const float kWanderStep = 0.5f;
const float kWanderPitchRange = 24.0f;  // semitones, as stock

class Wander {
 public:
  Wander() { }
  ~Wander() { }

  void Init() {
    for (int i = 0; i < 4; ++i) {
      position_[i] = 0.0f;
    }
  }

  // Rewrites the unpatched, counter-clockwise parameters as a fixed offset
  // for this block. Returns the spread they had, which the pan keeps.
  float Process(int steps, bool intervals, Parameters* p) {
    float spread = 0.0f;
    spread += Walk(0, steps, false, false, &p->time);
    spread += Walk(1, steps, false, false, &p->size);
    spread += Walk(2, steps, false, false, &p->shape);
    spread += Walk(3, steps, true, intervals, &p->pitch);
    return spread;
  }

 private:
  float Walk(
      int index,
      int steps,
      bool pitch,
      bool intervals,
      RandomizedParameter* p) {
    if (p->has_cv || p->attenuverter >= 0.0f) {
      return 0.0f;
    }
    float position = position_[index];
    for (int i = 0; i < steps; ++i) {
      position += kWanderStep * PseudoGaussian();
      if (position > 1.0f) {
        position = 2.0f - position;
      } else if (position < -1.0f) {
        position = -2.0f - position;
      }
    }
    position_[index] = position;
    const float amount = -p->attenuverter;
    float offset;
    if (pitch) {
      offset = amount * position * kWanderPitchRange;
      if (intervals) {
        offset = QuantizeFifths(offset);
      }
    } else {
      // As the clockwise side: a blend towards a point that now wanders.
      offset = amount * (0.5f + 0.5f * position - p->base);
    }
    p->attenuverter = 1.0f;
    p->cv = offset;
    p->has_cv = true;
    return amount;
  }

  float position_[4];

  DISALLOW_COPY_AND_ASSIGN(Wander);
};

}  // namespace beads

#endif  // BEADS_DSP_WANDER_H_

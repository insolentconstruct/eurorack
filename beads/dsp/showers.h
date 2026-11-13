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
// Random grain timing in showers and lulls: the rate of the randomly
// modulated DENSITY switches between bursts and gaps, same mean rate.

#ifndef BEADS_DSP_SHOWERS_H_
#define BEADS_DSP_SHOWERS_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace beads {

// Three times the rate a quarter of the time, a third of it otherwise.
const float kShowerOctaves = 1.5849625f;  // log2(3)
const float kShowerGrains = 8.0f;
const float kShowerMinDuration = 0.15f;  // seconds
const float kShowerMaxDuration = 1.0f;  // seconds
const float kLullRatio = 3.0f;

class Showers {
 public:
  Showers() { }
  ~Showers() { }

  void Init() {
    shower_ = false;
  }

  // Rate offset in octaves for a randomly modulated DENSITY.
  float Process(float density, float seconds) {
    // Mean grain rate of the clockwise half, as the trigger generator.
    const float rate = 65.40639f * stmlib::SemitonesToRatio(
        -(1.0f - density) * 192.0f);
    float duration = kShowerGrains / (rate * 3.0f);
    CONSTRAIN(duration, kShowerMinDuration, kShowerMaxDuration);
    if (!shower_) {
      duration *= kLullRatio;
    }
    if (stmlib::Random::GetFloat() * duration < seconds) {
      shower_ = !shower_;
    }
    return shower_ ? kShowerOctaves : -kShowerOctaves;
  }

 private:
  bool shower_;

  DISALLOW_COPY_AND_ASSIGN(Showers);
};

}  // namespace beads

#endif  // BEADS_DSP_SHOWERS_H_

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
// Wrapping granular TIME scan.

#ifndef BEADS_DSP_TIME_SCAN_H_
#define BEADS_DSP_TIME_SCAN_H_

#include <cmath>

#include "beads/dsp/randomized_parameter.h"

namespace beads {

class TimeScan {
 public:
  TimeScan() { }
  ~TimeScan() { }

  void Init() { position_ = 0.0f; }

  void Process(float seconds, RandomizedParameter* time) {
    // TIME reads backwards from the write head. Clockwise scans forwards.
    position_ -= time->attenuverter * seconds * 0.25f;
    position_ -= floorf(position_);
    float position = time->base + position_;
    position -= floorf(position);
    time->base = position;
    time->attenuverter = 0.0f;
    time->cv = 0.0f;
    time->has_cv = true;
  }

 private:
  float position_;

  DISALLOW_COPY_AND_ASSIGN(TimeScan);
};

}  // namespace beads

#endif  // BEADS_DSP_TIME_SCAN_H_

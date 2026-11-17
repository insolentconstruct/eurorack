// Copyright 2021 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Extract reliable clock information from taps or an external signal.

#ifndef BEADS_DSP_DELAY_CLOCK_H_
#define BEADS_DSP_DELAY_CLOCK_H_

#include <algorithm>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

namespace beads {

class DelayClock {
 public:
  DelayClock() { }
  ~DelayClock() { }
  
  void Init(float sample_rate);
  
  void Process(bool tick, bool tap);
  
  void Update(float interval, float lp_coefficient) {
    ONE_POLE(interval_[0], interval, lp_coefficient);
    interval_[1] = interval_[0];
    fade_ = 1.0f;
    rotate_ = 0;
  }

  // Knob timing that crossfades to the target instead of gliding.
  void Jump(float interval, float target, float lp_coefficient) {
    // A running crossfade finishes first.
    fade_ += fade_increment_;
    if (fade_ < 1.0f) {
      return;
    }
    fade_ = 1.0f;
    const float current = interval_[1];
    // Under 10 ms, half the shortest fade, keep the glide for flanging.
    if (std::max(target, current) * max_fade_increment_ < 0.5f) {
      ONE_POLE(interval_[1], interval, lp_coefficient);
      interval_[0] = interval_[1];
    } else if (fabsf(target - current) > 0.02f * current) {
      // As for a clock change: the new interval fades in over half of it.
      ++rotate_;
      interval_[0] = current;
      interval_[1] = target;
      fade_ = 0.0f;
      fade_increment_ = 2.0f / target;
      CONSTRAIN(fade_increment_, min_fade_increment_, max_fade_increment_);
      next_interval_ = 0;
    }
  }
  
  inline float fade() const {
    return fade_;
  }
  
  inline float interval(int i) const {
    i = (i + rotate_) % 2;
    return interval_[i];
  }
  
  inline float gain(int i) const {
    i = (i + rotate_) % 2;
    return i ? fade_ : 1.0f - fade_;
  }
  
  inline bool retrig() const {
    return fade_ == 0.0f && !next_interval_;
  }
  
  inline int new_interval_index() const {
    return (rotate_ + 1) % 2;
  }
  
#ifdef TEST
  inline int& test_next_interval() { return next_interval_; }
  inline int test_max_interval() const { return max_interval_; }
#endif  // TEST

 private:
  float interval_[2];
  float average_interval_;

  float fade_;
  float fade_increment_;
  float max_fade_increment_;
  float min_fade_increment_;

  float acceptable_jitter_;
  float jitter_sensitivity_;

  bool schedule_change_;

  int next_interval_;
  int max_interval_;
  
  int rotate_;

  DISALLOW_COPY_AND_ASSIGN(DelayClock);
};


}  // namespace beads

#endif  // BEADS_DSP_DELAY_CLOCK_H_

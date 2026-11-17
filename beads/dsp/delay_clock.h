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

#include "beads/resources.h"

namespace beads {

// DENSITY's filter moves in steps, so a sweep settles only after 50 ms.
const uint8_t kCrossfadeSettleBlocks = 200;

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

  // Knob timing, once per block: holds the delay time while DENSITY moves,
  // then crossfades to it. Returns true when it glides instead.
  bool Crossfade(float target, bool frozen) {
    if (fade_ < 1.0f) {
      return false;
    }
    const float current = interval_[1];
    // Frozen slices, and delays under 10 ms (flanging), glide as stock.
    if (frozen || std::max(target, current) * max_fade_increment_ < 0.5f) {
      interval_[0] = current;
      return true;
    }
    if (fabsf(target - current) <= 0.02f * current) {
      interval_[0] = current;
      settle_ = 0;
      return false;
    }
    // While no fade runs, the silent old interval holds the pending target.
    if (fabsf(target - interval_[0]) > 0.02f * interval_[0]) {
      interval_[0] = target;
      settle_ = 0;
      return false;
    }
    if (++settle_ < kCrossfadeSettleBlocks) {
      return false;
    }
    // As for a clock change: the new interval fades in over half of it.
    ++rotate_;
    interval_[0] = current;
    interval_[1] = target;
    fade_increment_ = 2.0f / target;
    CONSTRAIN(fade_increment_, min_fade_increment_, max_fade_increment_);
    fade_ = -fade_increment_;  // The block's first sample starts the fade.
    next_interval_ = 0;
    settle_ = 0;
    return false;
  }

  // Knob timing, every sample.
  inline void Fade() {
    fade_ += fade_increment_;
    if (fade_ >= 1.0f) {
      fade_ = 1.0f;
    }
  }

  inline void Glide(float interval, float lp_coefficient) {
    ONE_POLE(interval_[1], interval, lp_coefficient);
  }

  // Equal-power gains for knob-timing crossfades, at the block's first
  // sample, and their steps across the block.
  inline void PowerGains(size_t size, float* gain, float* step) const {
    const float first = std::min(fade_ + fade_increment_, 1.0f);
    const float last = std::min(fade_ + fade_increment_ * float(size), 1.0f);
    const float scale = size > 1 ? 1.0f / float(size - 1) : 0.0f;
    for (int i = 0; i < 2; ++i) {
      const bool fading_in = (i + rotate_) % 2;
      const float a = fading_in ? first : 1.0f - first;
      const float b = fading_in ? last : 1.0f - last;
      gain[i] = stmlib::Interpolate(lut_sine, a * 0.25f, 1024.0f);
      const float end = stmlib::Interpolate(lut_sine, b * 0.25f, 1024.0f);
      step[i] = (end - gain[i]) * scale;
    }
  }

  // Sum of the gains now, or 1.41 if the last 12 samples crossed mid-fade.
  inline float power_sum() const {
    if (fade_ >= 1.0f) {
      return 1.0f;
    } else if (fade_ >= 0.5f && fade_ - 12.0f * fade_increment_ < 0.5f) {
      return 1.4142135f;
    }
    return stmlib::Interpolate(lut_sine, fade_ * 0.25f, 1024.0f) + \
        stmlib::Interpolate(lut_sine, (1.0f - fade_) * 0.25f, 1024.0f);
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
  uint8_t settle_;  // Blocks DENSITY has held still, in the padding.

  int next_interval_;
  int max_interval_;
  
  int rotate_;

  DISALLOW_COPY_AND_ASSIGN(DelayClock);
};


}  // namespace beads

#endif  // BEADS_DSP_DELAY_CLOCK_H_

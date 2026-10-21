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
// -----------------------------------------------------------------------------
//
// Attacks in the recorded audio (#58), and grains triggered by them (#47).

#ifndef BEADS_DSP_ATTACK_DETECTOR_H_
#define BEADS_DSP_ATTACK_DETECTOR_H_

#include <algorithm>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

namespace beads {

enum AttackMode {
  ATTACK_MODE_OFF,
  ATTACK_MODE_SNAP,
  ATTACK_MODE_SNAP_AND_TRIGGER
};

// A block is an attack when its peak is this many times the recent level.
const float kAttackThreshold = 1.5f;
const float kAttackFloor = 0.002f;
const float kAttackRefractoryTime = 0.030f;  // seconds
const float kAttackReleaseRate = 20.0f;  // Hz
// Grain starts move back by up to this much to land on an attack.
const float kAttackSnapTime = 0.100f;  // seconds
const unsigned kNumAttackMarks = 64;

// Finds attacks in the codec input, one decision per block.
class AttackInput {
 public:
  AttackInput() { }
  ~AttackInput() { }

  void Init() {
    Configure(ATTACK_MODE_OFF, kAttackThreshold);
  }

  void Configure(int mode, float threshold) {
    mode_ = mode;
    threshold_ = threshold;
    Reset();
  }

  void Reset() {
    refractory_ = 0;
    reference_ = 0.5f;
  }

  inline int mode() const { return mode_; }

  // The input and the slow peak are both before the input gain, scaled to
  // the codec's full scale.
  template<typename Frame>
  bool Process(
      const Frame* in,
      size_t size,
      float scale,
      float slow,
      float sample_rate,
      bool left_normalized,
      bool right_normalized) {
    if (!mode_) {
      return false;
    }
    refractory_ = refractory_ > size ? refractory_ - size : 0;

    // Magnitudes compare as integers in the same order as floats: two maxima
    // per block, and one conversion at the end.
    uint32_t peak_l = 0;
    uint32_t peak_r = 0;
    if (!(left_normalized && right_normalized)) {
      for (size_t i = 0; i < size; ++i) {
        peak_l = std::max(peak_l, Magnitude(in[i].l));
        peak_r = std::max(peak_r, Magnitude(in[i].r));
      }
    }
    const uint32_t block_peak = std::max(
        left_normalized ? peak_r : peak_l,
        right_normalized ? peak_l : peak_r);
    const float peak = float(block_peak) * scale;
    const bool onset = !refractory_ &&
        peak > threshold_ * std::max(reference_, slow * 0.5f) &&
        peak > kAttackFloor;

    // Instant attack, release at the block rate.
    SLOPE(reference_, peak, 1.0f, kAttackReleaseRate * float(size) / sample_rate);
    if (onset) {
      refractory_ = unsigned(sample_rate * kAttackRefractoryTime);
    }
    return onset;
  }

 private:
  static inline uint32_t Magnitude(int32_t x) {
    return x < 0 ? -uint32_t(x) : uint32_t(x);
  }

  int mode_;
  float threshold_;
  float reference_;
  unsigned refractory_;

  DISALLOW_COPY_AND_ASSIGN(AttackInput);
};

// Remembers where the attacks are in the recording, and moves grain starts
// onto them.
class AttackDetector {
 public:
  AttackDetector() { }
  ~AttackDetector() { }

  void Init(float sample_rate) {
    sample_rate_ = sample_rate;
    mode_ = ATTACK_MODE_OFF;
    Reset();
  }

  void Configure(int mode) {
    mode_ = mode;
    Reset();
  }

  void Reset() {
    clock_ = 0;
    head_ = 0;
    count_ = 0;
    pending_ = false;
  }

  inline int mode() const { return mode_; }

  inline void SetOnset(bool onset) { pending_ = onset; }

  void BeginBlock(int write_ptr, unsigned capacity, bool granular, bool frozen) {
    start_ptr_ = write_ptr;
    capacity_ = capacity;
    // Forget the marks the recording has overwritten.
    while (count_ && clock_ - marks_[Older(count_)] >= capacity) {
      --count_;
    }
    if (!granular) {
      count_ = 0;
    }
    if (granular && !frozen && pending_) {
      marks_[head_] = clock_;
      head_ = (head_ + 1) % kNumAttackMarks;
      if (count_ < kNumAttackMarks) {
        ++count_;
      }
    }
    pending_ = false;
  }

  inline void EndBlock(unsigned writes) { clock_ += writes; }

  // The nearest attack at or before the requested start, within
  // kAttackSnapTime; otherwise the requested start.
  float Snap(float delay, float max_delay, int write_ptr) const {
    if (!count_ || delay < 0.0f || max_delay < delay) {
      return delay;
    }
    int delta = start_ptr_ - write_ptr;
    if (delta < 0) {
      delta += capacity_;
    }
    const uint32_t now = clock_ + unsigned(delta);
    // The oldest mark bounds the others.
    if (float(now - marks_[Older(count_)]) < delay) {
      return delay;
    }
    unsigned first = unsigned(delay);
    if (float(first) < delay) {
      ++first;
    }
    const unsigned limit = unsigned(
        std::min(delay + sample_rate_ * kAttackSnapTime, max_delay));
    // Ages grow from the newest mark to the oldest, even across a clock wrap.
    unsigned low = 0;
    unsigned high = count_;
    while (low < high) {
      const unsigned middle = (low + high) >> 1;
      if (now - marks_[Older(middle + 1)] < first) {
        low = middle + 1;
      } else {
        high = middle;
      }
    }
    if (low < count_) {
      const unsigned age = now - marks_[Older(low + 1)];
      if (age <= limit) {
        return float(age);
      }
    }
    return delay;
  }

 private:
  // The mark written n marks ago (n = 1 is the newest).
  inline unsigned Older(unsigned n) const {
    return (head_ + kNumAttackMarks - n) % kNumAttackMarks;
  }

  int mode_;
  int start_ptr_;
  unsigned capacity_;
  unsigned head_;
  unsigned count_;
  uint32_t clock_;
  uint32_t marks_[kNumAttackMarks];
  float sample_rate_;
  bool pending_;

  DISALLOW_COPY_AND_ASSIGN(AttackDetector);
};

}  // namespace beads

#endif  // BEADS_DSP_ATTACK_DETECTOR_H_

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
// Block-rate attack measurement prototype for #58/#47. No UI or persistence.
#ifndef BEADS_DSP_ATTACK_DETECTOR_H_
#define BEADS_DSP_ATTACK_DETECTOR_H_
#include <algorithm>
#include <cmath>
#include <stdint.h>
#include "stmlib/dsp/dsp.h"
namespace beads {
class AttackInput {
 public:
  void Init() { Configure(0, 1.5f); }
  void Configure(int mode, float threshold) {
    mode_ = mode; threshold_ = threshold; refractory_ = 0;
  }
  void Reset() { refractory_ = 0; }
  int mode() const { return mode_; }
  // Input and slow peak MUST both be pre-gain, normalized to codec full scale.
  // This optional pass never runs in Off. No second envelope follower.
  template<typename Frame> bool Process(const Frame* in, size_t size,
      float scale, float slow, float sample_rate,
      bool left_normalization = false, bool right_normalization = false) {
    if (!mode_) return false;
    if (refractory_ > size) refractory_ -= size; else refractory_ = 0;
    float peak = 0.0f;
    if (!(left_normalization && right_normalization)) {
      for (size_t i = 0; i < size; ++i) {
        float l = float(left_normalization ? in[i].r : in[i].l) * scale;
        float r = float(right_normalization ? in[i].l : in[i].r) * scale;
        peak = std::max(peak, std::max(fabsf(l), fabsf(r)));
      }
    }
    if (!refractory_ && peak > threshold_ * slow && peak > 0.002f) {
      refractory_ = unsigned(sample_rate * 0.030f);
      return true;
    }
    return false;
  }
  // Merge before Beads' TriggerGenerator, so held SEED wins naturally.
  static void MergeSeed(bool onset, stmlib::GateFlags* seed, size_t size) {
    if (!onset || !size) return;
    seed[0] = stmlib::GateFlags(seed[0] | stmlib::GATE_FLAG_HIGH | stmlib::GATE_FLAG_RISING);
  }
 private:
  int mode_;
  float threshold_;
  unsigned refractory_;
};
class AttackDetector {
 public:
  void Init(float sr) { sample_rate_ = sr; mode_ = 0; Reset(); }
  void Configure(int mode) { mode_ = mode; Reset(); }
  void Reset() { clock_ = head_ = count_ = 0; pending_ = false; }
  int mode() const { return mode_; }
  void SetOnset(bool onset) { pending_ = onset; }
  void BeginBlock(int write_ptr, unsigned capacity, bool granular, bool frozen) {
    start_ptr_ = write_ptr; capacity_ = capacity;
    while (count_ && clock_ - marks_[(head_ + 64 - count_) & 63] >= capacity) --count_;
    if (!granular) count_ = 0;
    if (granular && !frozen && pending_) {
      marks_[head_] = clock_; head_ = (head_ + 1) & 63;
      if (count_ < 64) ++count_;
    }
    pending_ = false;
  }
  void EndBlock(unsigned writes) { clock_ += writes; }
  float Snap(float delay, float max_delay, int write_ptr) const {
    int delta = start_ptr_ - write_ptr;
    if (delta < 0) delta += capacity_;
    const float limit = std::min(delay + sample_rate_ * 0.100f, max_delay);
    for (unsigned i = 0; i < count_; ++i) {
      float age = float(clock_ - marks_[(head_ + 63 - i) & 63] + unsigned(delta));
      if (age > limit) break;
      if (age >= delay) return age;
    }
    return delay;
  }
 private:
  int mode_, start_ptr_;
  unsigned capacity_, head_, count_;
  uint32_t clock_, marks_[64];
  float sample_rate_;
  bool pending_;
};
}  // namespace beads
#endif  // BEADS_DSP_ATTACK_DETECTOR_H_

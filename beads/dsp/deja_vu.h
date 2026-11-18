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
// Randomness with memory, after Marbles' deja vu: each event either records
// the random generator's state in a loop or replays a recorded one.

#ifndef BEADS_DSP_DEJA_VU_H_
#define BEADS_DSP_DEJA_VU_H_

#include "stmlib/stmlib.h"

#include "stmlib/utils/random.h"

namespace beads {

const int kDejaVuBufferSize = 16;

class DejaVu {
 public:
  DejaVu() { }
  ~DejaVu() { }

  void Init() {
    for (int i = 0; i < kDejaVuBufferSize; ++i) {
      loop_[i] = stmlib::Random::GetWord();
    }
    loop_write_head_ = 0;
    length_ = 8;
    step_ = 0;
    deja_vu_ = 0.0f;
    saved_state_ = 0;
    replaying_ = false;
  }

  // Marbles' NextValue: up to 0.5 the loop is rewritten less and less often;
  // past 0.5 it is replayed, then jumped through at random.
  inline void Begin() {
    const float p_sqrt = 2.0f * deja_vu_ - 1.0f;
    const bool mutate = stmlib::Random::GetFloat() < p_sqrt * p_sqrt;
    replaying_ = !mutate || deja_vu_ > 0.5f;
    if (!replaying_) {
      loop_[loop_write_head_] = stmlib::Random::state();
      loop_write_head_ = (loop_write_head_ + 1) % kDejaVuBufferSize;
      step_ = length_ - 1;
      return;
    }
    if (mutate) {
      step_ = static_cast<int>(stmlib::Random::GetFloat() * float(length_));
    } else {
      step_ = step_ + 1;
      if (step_ >= length_) {
        step_ = 0;
      }
    }
    const int i = loop_write_head_ + kDejaVuBufferSize - length_ + step_;
    saved_state_ = stmlib::Random::state();
    stmlib::Random::Seed(loop_[i % kDejaVuBufferSize]);
  }

  // Everything else continues the generator's own sequence.
  inline void End() {
    if (replaying_) {
      stmlib::Random::Seed(saved_state_);
    }
  }

  inline void set_deja_vu(float deja_vu) { deja_vu_ = deja_vu; }
  inline void set_length(int length) { length_ = length; }

 private:
  uint32_t loop_[kDejaVuBufferSize];
  int loop_write_head_;
  int length_;
  int step_;
  float deja_vu_;
  uint32_t saved_state_;
  bool replaying_;

  DISALLOW_COPY_AND_ASSIGN(DejaVu);
};

}  // namespace beads

#endif  // BEADS_DSP_DEJA_VU_H_

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
// Dictaphone medium (#221): automatic level control on the way in, a narrow
// tape band and hiss on every pass.

#ifndef BEADS_DSP_FX_DICTAPHONE_H_
#define BEADS_DSP_FX_DICTAPHONE_H_

#include <algorithm>
#include <cmath>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"

#include "beads/dsp/frame.h"

namespace beads {

const float kDictaphoneLowCut = 300.0f;
const float kDictaphoneHighCut = 3400.0f;
const float kDictaphoneLevel = 0.3f;
const float kDictaphoneMaxGain = 4.0f;
const float kDictaphoneHiss = 0.002f;
const float kDictaphoneWow = 0.35f;

class Dictaphone {
 public:
  Dictaphone() { }
  ~Dictaphone() { }

  void Init(float sample_rate) {
    for (int i = 0; i < 2; ++i) {
      low_cut_[i].Init();
      low_cut_[i].set_f_q<stmlib::FREQUENCY_FAST>(
          kDictaphoneLowCut / sample_rate, 0.7f);
      high_cut_[i].Init();
      high_cut_[i].set_f_q<stmlib::FREQUENCY_FAST>(
          kDictaphoneHighCut / sample_rate, 0.9f);
    }
    attack_ = 200.0f / sample_rate;
    release_ = 1.0f / sample_rate;
    level_ = 0.0f;
    noise_ = 1;
  }

  // The microphone input: the gain pumps up quiet passages and the hiss.
  inline void Level(FloatFrame* in) {
    SLOPE(level_, std::max(fabsf(in->l), fabsf(in->r)), attack_, release_);
    const float gain = std::min(
        kDictaphoneLevel / (level_ + kDictaphoneLevel / kDictaphoneMaxGain),
        kDictaphoneMaxGain);
    noise_ = noise_ * 1664525U + 1013904223U;
    const float hiss = float(int32_t(noise_)) * (kDictaphoneHiss / 2147483648.0f);
    in->l = (in->l + hiss) * gain;
    in->r = (in->r - hiss) * gain;
  }

  // What reaches the tape, including every pass of FEEDBACK.
  inline void Record(FloatFrame* s) {
    s->l = high_cut_[0].Process<stmlib::FILTER_MODE_LOW_PASS>(
        low_cut_[0].Process<stmlib::FILTER_MODE_HIGH_PASS>(s->l));
    s->r = high_cut_[1].Process<stmlib::FILTER_MODE_LOW_PASS>(
        low_cut_[1].Process<stmlib::FILTER_MODE_HIGH_PASS>(s->r));
  }

 private:
  stmlib::Svf low_cut_[2];
  stmlib::Svf high_cut_[2];
  float attack_;
  float release_;
  float level_;
  uint32_t noise_;

  DISALLOW_COPY_AND_ASSIGN(Dictaphone);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_DICTAPHONE_H_

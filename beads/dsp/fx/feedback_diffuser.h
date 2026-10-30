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
// Wet feedback while FREEZE is on, independent of the frozen recording buffer.

#ifndef BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_
#define BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_

#include <algorithm>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/randomized_parameter.h"
#include "beads/dsp/fx/diffuser.h"
#include "beads/dsp/fx/fx_engine.h"

namespace beads {

// Two long all-passes, one per channel, 32 ms and 43 ms; each is read through
// a tap that wanders by up to 1.5 ms (72 samples).
const size_t kFeedbackDelayBufferSize = 4096;
const float kFeedbackDelayModulationDepth = 72.0f;  // Samples
const float kFeedbackDelayCoefficient = 0.625f;

// Each tap wanders between random targets; one segment lasts about this long,
// and the three periods differ so that the taps do not move together.
const float kFeedbackWanderPeriodL = 96000.0f;  // Samples, about 2 s
const float kFeedbackWanderPeriodR = 120000.0f;
const float kFeedbackWanderPeriodDiffuser = 144000.0f;

const float kFeedbackEngageSlew = 0.00035f;  // Per sample, about 60 ms
const size_t kFeedbackClearSlice = 512;  // Samples per idle block

// A decaying float that is scaled by more than one half rounds back up to the
// smallest subnormal and stays there; the loop state is cleared below this.
const float kFeedbackDenormal = 1.0e-20f;

// Loop gain follows FEEDBACK up to the knee, then rises at a reduced slope to
// a ceiling of 0.7. The loop output is matched to the level of its input by a
// slow power tracker, and mixed with the input at constant power; the wet
// share follows a smooth step of FEEDBACK, so that the attacks of the input
// are mostly gone at high settings and untouched at low ones.
const float kFeedbackLoopGain = 0.85f;
const float kFeedbackLoopKnee = 0.51f;
const float kFeedbackLoopSlope = 0.56f;
const float kFeedbackPowerSlew = 0.0005f;  // Per block, about 0.5 s
const float kFeedbackMatchMin = 0.25f;
const float kFeedbackMatchMax = 4.0f;

class FeedbackDiffuser {
 public:
  FeedbackDiffuser() { }
  ~FeedbackDiffuser() { }

  void Init() {
    diffuser_.Init(diffuser_buffer_);
    diffuser_.set_amount(1.0f);
    delay_.Init(delay_buffer_);
    delay_write_ptr_ = 0;
    previous_.l = previous_.r = 0.0f;
    engaged_ = 0.0f;
    regeneration_ = 0.0f;
    wet_amount_ = 0.0f;
    dry_amount_ = 1.0f;
    match_ = 1.0f;
    match_target_ = 1.0f;
    input_power_ = 0.0f;
    wet_power_ = 0.0f;
    clear_ = kClearEnd;
    dirty_ = false;
    wander_[0].Init(1.0f / (4.0f * kFeedbackWanderPeriodL));
    wander_[1].Init(1.0f / (4.0f * kFeedbackWanderPeriodR));
    wander_[2].Init(1.0f / (4.0f * kFeedbackWanderPeriodDiffuser));
  }

  // Faded out and cleared: Process would do nothing.
  inline bool idle() const {
    return !dirty_ && engaged_ <= 0.0f && clear_ >= kClearEnd;
  }

  void Process(FloatFrame* in_out, float feedback, bool frozen, size_t size) {
    // Nothing runs while FREEZE is off, other than clearing the memory a slice
    // at a time once the loop has faded out.
    if (!frozen && engaged_ <= 0.0f) {
      if (dirty_) {
        // The interpolators write their state back when a block ends, so the
        // loop is reset here, once it is idle.
        dirty_ = false;
        clear_ = 0;
        input_power_ = wet_power_ = 0.0f;
        match_ = match_target_ = 1.0f;
      }
      Clear();
      return;
    }
    CONSTRAIN(feedback, 0.0f, 1.0f);
    // A freeze that comes back before the slices are done finishes the job.
    while (clear_ < kClearEnd) {
      Clear();
    }
    dirty_ = true;
    float engaged = engaged_ + \
        (frozen ? kFeedbackEngageSlew : -kFeedbackEngageSlew) * size;
    CONSTRAIN(engaged, 0.0f, 1.0f);
    engaged_ = engaged;

    const float mix = stmlib::SmoothStep(feedback) * engaged;
    stmlib::ParameterInterpolator regeneration(
        &regeneration_, LoopGain(feedback * engaged), size);
    stmlib::ParameterInterpolator wet_amount(&wet_amount_, mix, size);
    stmlib::ParameterInterpolator dry_amount(
        &dry_amount_, stmlib::Sqrt(1.0f - mix * mix), size);
    stmlib::ParameterInterpolator match(&match_, match_target_, size);

    // The taps move slowly: one position per block.
    DelayL apl;
    DelayR apr;
    const float depth_l = kFeedbackDelayModulationDepth * Wander(0, size);
    const float depth_r = kFeedbackDelayModulationDepth * Wander(1, size);
    const float offset_l = float(DelayL::length - 2) - depth_l;
    const float offset_r = float(DelayR::length - 2) - depth_r;
    const float diffuser_modulation = Wander(2, size);

    float input_power = 0.0f;
    float wet_power = 0.0f;
    int32_t write_ptr = delay_write_ptr_;
    while (size--) {
      write_ptr = delay_.Previous(write_ptr);
      // The tails of the long all-passes do not depend on the loop signal
      // either: they are read before the diffuser writes.
      const float tail_l = delay_.Tail(apl, write_ptr, offset_l);
      const float tail_r = delay_.Tail(apr, write_ptr, offset_r);

      const float r = regeneration.Next();
      input_power += in_out->l * in_out->l + in_out->r * in_out->r;
      FloatFrame wet = *in_out * (1.0f - r) + previous_ * r;
      diffuser_.Run<true>(&wet, 1, diffuser_modulation);
      wet.l = delay_.AllPass(
          apl, write_ptr, wet.l, tail_l, kFeedbackDelayCoefficient);
      wet.r = delay_.AllPass(
          apr, write_ptr, wet.r, tail_r, kFeedbackDelayCoefficient);
      previous_ = wet;
      wet_power += wet.l * wet.l + wet.r * wet.r;
      *in_out = *in_out * dry_amount.Next() + \
          wet * (wet_amount.Next() * match.Next());
      ++in_out;
    }
    delay_write_ptr_ = write_ptr;

    // Both powers are tracked, so that silence in and out leaves the match
    // where it was.
    ONE_POLE(input_power_, input_power, kFeedbackPowerSlew);
    ONE_POLE(wet_power_, wet_power, kFeedbackPowerSlew);
    if (input_power_ > 1.0e-9f && wet_power_ > 1.0e-9f) {
      match_target_ = stmlib::Sqrt(input_power_ / wet_power_);
      CONSTRAIN(match_target_, kFeedbackMatchMin, kFeedbackMatchMax);
    }
    if (fabsf(previous_.l) < kFeedbackDenormal) {
      previous_.l = 0.0f;
    }
    if (fabsf(previous_.r) < kFeedbackDenormal) {
      previous_.r = 0.0f;
    }
    if (input_power_ < kFeedbackDenormal) {
      input_power_ = 0.0f;
    }
    if (wet_power_ < kFeedbackDenormal) {
      wet_power_ = 0.0f;
    }
    if (engaged <= 0.0f) {
      engaged_ = 0.0f;
      previous_.l = previous_.r = 0.0f;
    }
  }

 private:
  typedef FxEngine<kFeedbackDelayBufferSize, FORMAT_16_BIT> E;
  typedef E::Reserve<1536, E::Reserve<2048> > DelayMemory;  // L, R
  typedef E::DelayLine<DelayMemory, 0> DelayL;
  typedef E::DelayLine<DelayMemory, 1> DelayR;

  static const size_t kClearEnd = \
      kFeedbackDelayBufferSize + kDiffuserBufferSize;

  static inline float LoopGain(float feedback) {
    float gain = kFeedbackLoopGain * feedback;
    if (gain > kFeedbackLoopKnee) {
      gain = kFeedbackLoopKnee + \
          (gain - kFeedbackLoopKnee) * kFeedbackLoopSlope;
    }
    return gain;
  }

  // A random walk in 0..1, as Beads uses for its own random modulation.
  inline float Wander(int index, size_t size) {
    return 0.5f + 0.5f * wander_[index].value(1.0f, size);
  }

  // Idle: zero the loop memory a slice per block, so that nothing stale comes
  // out at the next freeze and no block pays for it all at once.
  void Clear() {
    if (clear_ >= kClearEnd) {
      return;
    }
    const size_t end = std::min(
        clear_ + kFeedbackClearSlice, size_t(kClearEnd));
    // The slice size divides the size of the first buffer: no slice straddles.
    if (clear_ < kFeedbackDelayBufferSize) {
      std::fill(&delay_buffer_[clear_], &delay_buffer_[end], 0);
    } else {
      std::fill(
          &diffuser_buffer_[clear_ - kFeedbackDelayBufferSize],
          &diffuser_buffer_[end - kFeedbackDelayBufferSize],
          0);
    }
    clear_ = end;
  }

  Diffuser diffuser_;
  uint16_t diffuser_buffer_[kDiffuserBufferSize];
  AllPassMemory<kFeedbackDelayBufferSize> delay_;
  uint16_t delay_buffer_[kFeedbackDelayBufferSize];
  int32_t delay_write_ptr_;
  FloatFrame previous_;
  float engaged_;
  float regeneration_;
  float wet_amount_;
  float dry_amount_;
  float match_;
  float match_target_;
  float input_power_;
  float wet_power_;
  size_t clear_;
  bool dirty_;  // The memory has been written since it was last cleared.
  RandomWalkModulation wander_[3];  // Long all-pass L, R, diffuser tap

  DISALLOW_COPY_AND_ASSIGN(FeedbackDiffuser);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_FEEDBACK_DIFFUSER_H_

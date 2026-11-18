// Copyright 2014 Emilie Gillet.
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
// Resonestor: two voices of four tuned feedback combs, from Matthias Puech's
// Parasites firmware for Clouds (MIT); ported to Beads' sample rates.

#ifndef BEADS_DSP_RESONESTOR_H_
#define BEADS_DSP_RESONESTOR_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/filter.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/fx/fx_engine.h"

namespace beads {

// Sizes for 48 kHz; lower rates use part of each line.
const int kResonestorMaxComb = 1500;
const size_t kResonestorBufferSize = 32768;

class Resonestor {
 public:
  Resonestor() { }
  ~Resonestor() { }

  void Init(float* buffer, float sample_rate);
  void Process(FloatFrame* in_out, size_t size);

  void set_pitch(float pitch) {
    pitch_[voice_] = pitch;
  }

  void set_chord(float chord) {
    chord_[voice_] = chord;
  }

  void set_feedback(float feedback) {
    feedback_[voice_] = feedback;
  }

  void set_narrow(float narrow) {
    narrow_[voice_] = narrow;
  }

  void set_damp(float damp) {
    damp_[voice_] = damp;
  }

  void set_distortion(float distortion) {
    distortion *= distortion * distortion;
    distortion_[voice_] = distortion;
  }

  void set_trigger(bool trigger) {
    previous_trigger_ = trigger_;
    trigger_ = trigger;
  }

  void set_burst_damp(float burst_damp) {
    burst_lp_.set_f_q<stmlib::FREQUENCY_FAST>(
        burst_damp * burst_damp * 0.5f, 0.8f);
  }

  void set_burst_comb(float burst_comb) {
    burst_comb_ = burst_comb;
  }

  void set_burst_duration(float burst_duration) {
    burst_duration_ = burst_duration;
  }

  void set_spread_amount(float spread_amount) {
    spread_amount_ = spread_amount;
  }

  void set_stereo(float stereo) {
    stereo_ = stereo;
  }

  void set_separation(float separation) {
    separation_ = separation;
  }

  void set_freeze(bool freeze) {
    previous_freeze_ = freeze_;
    freeze_ = freeze;
  }

  void set_harmonicity(float harmonicity) {
    harmonicity_[voice_] = harmonicity;
  }

 private:
  typedef FxEngine<kResonestorBufferSize, FORMAT_32_BIT> E;
  E engine_;

  float sample_rate_;

  // Parameters.
  float feedback_[2];
  float pitch_[2];
  float chord_[2];
  float narrow_[2];
  float damp_[2];
  float harmonicity_[2];
  float distortion_[2];
  float spread_amount_;
  float stereo_;
  float separation_;
  float burst_time_;
  float burst_comb_;
  float burst_duration_;
  bool trigger_;
  bool previous_trigger_;
  bool freeze_;
  bool previous_freeze_;

  // Internal state.
  float spread_delay_[3];
  float comb_period_[4][2];
  float comb_feedback_[4][2];

  float hp_[4][2];
  stmlib::Svf lp_[4][2];
  stmlib::Svf bp_[4][2];
  stmlib::Svf burst_lp_;
  stmlib::Svf rand_lp_;
  stmlib::OnePole rand_hp_;

  int32_t voice_;

  DISALLOW_COPY_AND_ASSIGN(Resonestor);
};

}  // namespace beads

#endif  // BEADS_DSP_RESONESTOR_H_

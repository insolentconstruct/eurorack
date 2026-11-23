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
// Fades and blanking around codec restarts, engine and reverb style changes.

#ifndef BEADS_ENGINE_TRANSITION_H_
#define BEADS_ENGINE_TRANSITION_H_

#include "stmlib/stmlib.h"

#include "beads/drivers/codec.h"

// Audio callback defined in beads.cc.
void Process(
    const beads::Codec::Event* events,
    const beads::Codec::Frame* in,
    beads::Codec::Frame* out,
    size_t size);

namespace beads {

const int kBlockRate = 4000;
const float kReverbStyleFadeStep = 1.0f / (0.01f * kBlockRate);

class EngineTransition {
 public:
  EngineTransition() { }
  ~EngineTransition() { }

  inline void Init() {
    clouds_style_ = false;
    reverb_style_gain_ = 1.0f;
    reverb_style_previous_gain_ = 1.0f;
    codec_fade_ = false;
    codec_running_ = false;
    codec_sample_rate_ = 0;
  }

  inline int codec_transition_frames() const {
    return codec_transition_frames_;
  }
  inline float reverb_style_gain() const { return reverb_style_gain_; }
  inline bool codec_running() const { return codec_running_; }
  inline int codec_sample_rate() const { return codec_sample_rate_; }

  inline void set_codec_transition_frames(int value) {
    codec_transition_frames_ = value;
  }
  inline void set_codec_fade(bool value) {
    codec_fade_ = value;
  }
  inline void set_codec_running(bool value) {
    codec_running_ = value;
  }
  inline void set_codec_sample_rate(int value) {
    codec_sample_rate_ = value;
  }

 private:
  // The callback keeps its fades and blanking inline: helpers change its code.
  friend void ::Process(
      const beads::Codec::Event* events,
      const beads::Codec::Frame* in,
      beads::Codec::Frame* out,
      size_t size);

  // Static members stay separate objects so the code reading them is unchanged.
  // This counter is used to temporarily blank the codec output and disable
  // the AGC while the normalization detection system is still catching up with
  // a codec restart.
  static int codec_transition_frames_;
  static bool clouds_style_;
  static float reverb_style_gain_;
  // Set by main before it restarts the codec: the output fades out first.
  static volatile bool codec_fade_;
  static bool codec_running_;
  // The rate the converter runs at; 0 before its first start.
  static int codec_sample_rate_;
  static float reverb_style_previous_gain_;

  DISALLOW_COPY_AND_ASSIGN(EngineTransition);
};

}  // namespace beads

#endif  // BEADS_ENGINE_TRANSITION_H_

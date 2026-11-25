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
// Fades, blanking and codec restarts between engines, synths and styles.

#ifndef BEADS_ENGINE_TRANSITION_H_
#define BEADS_ENGINE_TRANSITION_H_

#include <algorithm>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/blob/synth_choice.h"
#include "beads/drivers/codec.h"
#include "beads/dsp/plaits_synth.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/rings_synth.h"
#include "beads/dsp/synth.h"

namespace beads {

class Ui;

const int kBlockRate = 4000;
const float kReverbStyleFadeStep = 1.0f / (0.01f * kBlockRate);

class EngineTransition {
 public:
  EngineTransition() { }
  ~EngineTransition() { }

  void Init(Codec* codec, PlaitsSynth* plaits_synth, RingsSynth* rings_synth,
      Ui* ui, uint8_t* bank, size_t bank_size);

  inline bool Blanking(Codec::Frame* out, size_t size) {
    if (!codec_transition_frames_) {
      return false;
    }
    --codec_transition_frames_;
    Codec::Frame silence;
    silence.l = 0;
    silence.r = 0;
    std::fill(&out[0], &out[size], silence);
    return true;
  }

  inline void SetCloudsStyle(bool clouds, Processor* processor, Synth* synth,
      PlaitsSynth* plaits_synth, RingsSynth* rings_synth, bool clear) {
    clouds_style_ = clouds;
    processor->SetCloudsStyle(clouds);
    synth->SetCloudsStyle(clouds, clear);
    plaits_synth->SetCloudsStyle(clouds, clear);
    rings_synth->SetCloudsStyle(clouds, clear);
  }

  inline bool Hold(bool requested_style, bool engine_change,
      Processor* processor, Synth* synth, PlaitsSynth* plaits_synth,
      RingsSynth* rings_synth, Codec::Frame* out, size_t size) {
    reverb_style_previous_gain_ = reverb_style_gain_;
    if (requested_style != clouds_style_ || engine_change || codec_fade_) {
      reverb_style_gain_ = std::max(
          0.0f, reverb_style_gain_ - kReverbStyleFadeStep);
      if (reverb_style_previous_gain_ == 0.0f) {
        if (requested_style != clouds_style_) {
          SetCloudsStyle(requested_style, processor, synth,
              plaits_synth, rings_synth, false);
        }
        Codec::Frame silence;
        silence.l = 0;
        silence.r = 0;
        std::fill(&out[0], &out[size], silence);
        return true;
      }
    } else if (reverb_style_gain_ < 1.0f) {
      reverb_style_gain_ = std::min(
          1.0f, reverb_style_gain_ + kReverbStyleFadeStep);
    }
    return false;
  }

  inline void Apply(FloatFrame* out, size_t size) {
    if (reverb_style_previous_gain_ != 1.0f || reverb_style_gain_ != 1.0f) {
      stmlib::ParameterInterpolator gain(
          &reverb_style_previous_gain_, reverb_style_gain_, size);
      for (size_t i = 0; i < size; ++i) {
        out[i] = out[i] * gain.Next();
      }
    }
  }

  void Start(int sample_rate, Codec::FillBufferCallback callback,
      bool blank_output);
  bool RetryCodec();
  void Pause();
  void TakeBank(SynthEngine owner);
  void SwapPaused(SynthEngine owner, SynthEngine playing,
      Codec::FillBufferCallback callback);

  // Codec restarts fade the output out first: true once it is silent.
  inline bool Fade() {
    // Before the first start there is no audio to fade.
    codec_fade_ = codec_running_;
    return !codec_running_ || reverb_style_gain_ == 0.0f;
  }

  inline void Cancel() { codec_fade_ = false; }

  inline bool blanking() const { return codec_transition_frames_ != 0; }
  inline bool faded_out() const { return reverb_style_gain_ == 0.0f; }
  inline bool clouds_style() const { return clouds_style_; }
  inline SynthEngine playing_synth() const { return playing_synth_; }
  inline void set_playing_synth(SynthEngine synth) { playing_synth_ = synth; }
  inline SynthEngine bank_owner() const { return bank_owner_; }
  inline void set_bank_owner(SynthEngine owner) { bank_owner_ = owner; }

 private:
  void StartCodec(int sample_rate, Codec::FillBufferCallback callback);

  // Blank codec output while normalization detection catches up with a restart.
  int codec_transition_frames_;
  bool clouds_style_;
  float reverb_style_gain_;
  // Set by main before it restarts the codec: the output fades out first.
  volatile bool codec_fade_;
  bool codec_running_;
  // The rate the converter runs at; 0 before its first start.
  int codec_sample_rate_;
  int codec_retry_rate_;
  uint32_t codec_retry_time_;
  Codec::FillBufferCallback codec_retry_callback_;
  float reverb_style_previous_gain_;
  SynthEngine playing_synth_;
  SynthEngine bank_owner_;
  Codec* codec_;
  PlaitsSynth* plaits_synth_;
  RingsSynth* rings_synth_;
  Ui* ui_;
  uint8_t* bank_;
  size_t bank_size_;

  DISALLOW_COPY_AND_ASSIGN(EngineTransition);
};

}  // namespace beads

#endif  // BEADS_ENGINE_TRANSITION_H_

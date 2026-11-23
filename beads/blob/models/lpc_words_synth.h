// Copyright 2016 Emilie Gillet.
// Copyright 2026 Insolent Construct.
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
// LPC10 speech synth.

#ifndef BEADS_BLOB_MODELS_LPC_WORDS_SYNTH_H_
#define BEADS_BLOB_MODELS_LPC_WORDS_SYNTH_H_

#include "stmlib/dsp/dsp.h"

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/speech/lpc_speech_synth.h"

namespace plaits {

const int kLPCWordsSynthOrder = 10;

const float kLPCWordsSynthDefaultF0 = 100.0f;

class LPCWordsSynth {
 public:
  LPCWordsSynth() { }
  ~LPCWordsSynth() { }

  typedef LPCSpeechSynth::Frame Frame;

  void Init();

  void Render(
      float prosody_amount,
      float pitch_shift,
      float* excitation,
      float* output,
      size_t size);

  void PlayFrame(const Frame* frames, float frame, bool interpolate) {
    MAKE_INTEGRAL_FRACTIONAL(frame);

    if (interpolate) {
      PlayFrame(
          frames[frame_integral],
          frames[frame_integral + 1],
          frame_fractional);
    } else {
      // Discrete playback may select the final frame.
      PlayFrame(frames[frame_integral], frames[frame_integral], 0.0f);
    }
  }

 private:
  void PlayFrame(const Frame& f1, const Frame& f2, float blend);

  template <int scale, typename X>
  float BlendCoefficient(X a, X b, float blend) {
    float a_f = static_cast<float>(a) / float(scale);
    float b_f = static_cast<float>(b) / float(scale);
    return a_f + (b_f - a_f) * blend;
  }

  float phase_;
  float frequency_;
  float noise_energy_;
  float pulse_energy_;

  float next_sample_;
  int excitation_pulse_sample_index_;

  float k_[kLPCWordsSynthOrder];
  float s_[kLPCWordsSynthOrder + 1];

  DISALLOW_COPY_AND_ASSIGN(LPCWordsSynth);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_LPC_WORDS_SYNTH_H_

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
// A modulated feedback delay network using the recording banks.

#ifndef BEADS_DSP_FX_ABYSS_H_
#define BEADS_DSP_FX_ABYSS_H_

#include "beads/dsp/frame.h"

namespace beads {

class Abyss {
 public:
  struct Controls {
    float room;
    float decay;
    float drift;
    float diffusion;
    float gravity;
    float pitch;
    float tone;
    float mix;
  };

  struct Pitch {
    float detune;
    float detune_mix;
    float fifth;
    float fifth_mix;
    float octave;
    float octave_mix;
  };

  static bool PitchOnNotch(float position);
  static Pitch MapPitch(float position);

  Abyss() { }
  ~Abyss() { }

  void Init(uint8_t* bank_0, uint8_t* bank_1, float rate);
  void Process(const Controls& controls, const bool* freeze,
      const FloatFrame* in, FloatFrame* out, size_t size);

 private:
  enum {
    kLines = 8,
    kLinesInBank0 = 5,
    kLineSize = 21504,
    kDiffuserSize = 2048,
  };

  STATIC_ASSERT((kLinesInBank0 * (kLineSize + kDiffuserSize)) *
      sizeof(float) <= 512 * 1024, abyss_bank_0);
  STATIC_ASSERT((kLines - kLinesInBank0) * kLineSize *
      sizeof(float) <= 256 * 1024, abyss_bank_1);

  float Read(int line, float delay) const;
  float Diffuse(int stage, float input, int delay, float amount);
  float Shift(float* phase, float step, float base, float width) const;
  void Mix(float* taps) const;

  float* lines_[kLines];
  float damping_[kLines];
  float phase_[kLines];
  float frozen_delay_[kLines];
  float sample_rate_;
  float room_slew_;
  float control_slew_;
  float damping_coefficient_;
  float onset_coefficient_;
  float room_;
  float wet_;
  float frozen_;
  float onset_floor_;
  float elapsed_;
  float pitch_phase_[6];
  float pitch_mix_[5];
  float detune_step_[2];
  float gravity_gain_;
  bool was_frozen_;
  int write_;
  int diffusion_write_;

  DISALLOW_COPY_AND_ASSIGN(Abyss);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_ABYSS_H_

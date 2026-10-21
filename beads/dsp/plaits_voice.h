// Copyright 2016 Emilie Gillet.
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
// One Plaits voice for the synth mode, adapted from plaits/dsp/voice.h: the
// model and note are set per note, there is no AUX output, no user data and
// no trigger delay, and FREEZE holds the low-pass gate open.

#ifndef BEADS_DSP_PLAITS_VOICE_H_
#define BEADS_DSP_PLAITS_VOICE_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/limiter.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/envelope.h"
#include "plaits/dsp/fx/low_pass_gate.h"

namespace beads {

const int kNumPlaitsModels = 24;

// A voice's engines and the RAM they take turns in. The engines stay out of
// this header: Plaits' and Beads' resources.h define clashing macros.
const size_t kPlaitsVoiceRamSize = 28 * 1024;

struct PlaitsEngines;

struct PlaitsPatch {
  int model;
  float note;
  float harmonics;
  float timbre;
  float morph;
};

class PlaitsVoice {
 public:
  PlaitsVoice() { }
  ~PlaitsVoice() { }

  // Builds the engines in kPlaitsVoiceRamSize bytes of ram.
  void Init(uint8_t* ram);

  // trigger takes plaits::TriggerState flags; size is at most 24 samples.
  void Render(
      const PlaitsPatch& patch,
      int trigger,
      bool sustain,
      float decay,
      int bit_shift,
      float* out,
      size_t size);

  inline int model() const { return model_; }

 private:
  PlaitsEngines* engines_;
  int model_;

  bool sustain_;
  float sustain_level_;
  plaits::LPGEnvelope lpg_envelope_;
  plaits::LowPassGate lpg_;
  stmlib::Limiter limiter_;

  DISALLOW_COPY_AND_ASSIGN(PlaitsVoice);
};

}  // namespace beads

#endif  // BEADS_DSP_PLAITS_VOICE_H_

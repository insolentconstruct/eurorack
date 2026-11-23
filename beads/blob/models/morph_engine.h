// Copyright 2012 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// Morph: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_MORPH_ENGINE_H_
#define BEADS_BLOB_MODELS_MORPH_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// The port runs its inner chain at Braids' own 96 kHz so the one-pole
const int kMorphOversampling = 2;

// macro_oscillator.cc:74 and :80. The region boundaries are on the raw 15-bit
const float kMorphRegion1End = 10922.0f;
const float kMorphRegion2Start = 10923.0f;
const float kMorphRegion2End = 21845.0f;
const float kMorphRegion3Start = 21846.0f;

// `balance = parameter_[0] * 6` (:79) and `65535 - (parameter_[0] - 10923) * 6`
const float kMorphBalanceSlope = 6.0f / 65535.0f;

// :87 scales the third region by 3 into RenderSquare, which clamps at 32000
const float kMorphSquareSlope = 3.0f;
const float kMorphSquareMax = 32000.0f;

// :99 -- `lp_cutoff = pitch_ - (parameter_[1] >> 1) + 128 * 128`, clamped to
const float kMorphCutoffCeiling = 128.0f;
const float kMorphCutoffSpan = 32767.0f / 256.0f;
const float kMorphCutoffMax = 32767.0f / 128.0f;

// lookup_tables.py:101-103 clamps the table's cutoff at 1/8 of the sample
const float kMorphCutoffLimit = 0.125f;

// :107-113 -- `fuzz_amount = parameter_[1] << 1`, then above MIDI 80 it loses
const float kMorphFuzzGuardNote = 80.0f;
const float kMorphFuzzRollOff = 2048.0f / 65535.0f;

// MORPH takes the cutoff five octaves either side of where Braids' coupling
const float kMorphMaxToneOffset = 60.0f;

// MACRO's drive range around Braids' welded 1.0. Six octaves of gain, because
const float kMorphMinDrive = 0.125f;
const float kMorphMaxDrive = 8.0f;

// The two stereo channels sit this far either side of the Fuzz knob. The knob
const float kMorphStereoFuzz = 0.14f;

// 31-tap Kaiser(beta = 7.5) halfband, normalised to unity DC gain. Every even
const float kMorphHalfbandCentre = 0.499977286f;
const float kMorphHalfband1 = 0.313391108f;
const float kMorphHalfband3 = -0.092152672f;
const float kMorphHalfband5 = 0.042744897f;
const float kMorphHalfband7 = -0.020375131f;
const float kMorphHalfband9 = 0.008867136f;
const float kMorphHalfband11 = -0.003211545f;
const float kMorphHalfband13 = 0.000826695f;
const float kMorphHalfband15 = -0.000079130f;

// Braids caps its pitch at kHighestNote = 128 * 128 in ComputePhaseIncrement
const float kMorphMaxNote = 128.0f;

class Morph {
 public:
  Morph() { }
  ~Morph() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);


 private:
  inline float Decimate(const float* history, int position) const;

  float phase_;
  float frequency_;

  // The crossfade weights, the pulse width and the two fuzz mixes, all
  float saw_;
  float square_;
  float pw_;
  float fuzz_;
  float fuzz_aux_;

  // One band-limited oscillator feeds both channels; only the post-oscillator
  bool high_;
  float previous_pw_;
  float next_sample_;

  float lp_state_;
  float lp_state_aux_;

  float dc_input_;
  float dc_input_aux_;

  // Two mirrored copies turn each 31-tap decimator window into one contiguous
  float history_[64];
  float history_aux_[64];
  int history_position_;

  DISALLOW_COPY_AND_ASSIGN(Morph);
};

class MorphEngine : public Engine {
 public:
  MorphEngine() { }
  ~MorphEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    morph_.Init(allocator);
  }

  virtual void Reset() {
    morph_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    morph_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Morph morph_;

  DISALLOW_COPY_AND_ASSIGN(MorphEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_MORPH_ENGINE_H_

// Copyright 1995-2023 Perry R. Cook and Gary P. Scavone.
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Brass: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_BRASS_ENGINE_H_
#define BEADS_BLOB_MODELS_BRASS_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// Bore length at the lowest note, longest slide, with the pull compensation:
const int kBrassDelaySize = 2688;

// Below this the engine folds up by octaves rather than allocating for a
const float kBrassLowestNote = 33.0f;  // A1, 55 Hz.

// The played note is the bore's SECOND partial (finding 6).
const int kBrassFirstPartial = 2;
const int kBrassLastPartial = 7;

// A partial is dropped rather than offered if it would sit above this.
const float kBrassHighestPartial = 3000.0f;

// Where inside a partial's lock zone the lip sits, as a multiple of that
const float kBrassZoneBase = 0.935f;
const float kBrassZoneSpan = 0.025f;

// The lip's pitch pull, in cents, as a function of partial number: it fits
const float kBrassPullOffset = -4.8f;
const float kBrassPullScale = 67.2f;

// Valve and bore constants, from the parameter search. The lip Q is not a taste
const float kBrassLipZeta = 0.04f;
const float kBrassLipStiffness = 3.0f;
// The valve's rest opening at MACRO 0.5. MACRO scales it over [0.5, 1.5] of
const float kBrassRestOpening = 0.60f;
const float kBrassMaxOpening = 1.80f;
const float kBrassBoreImpedance = 0.15f;
const float kBrassReflection = 0.995f;

// Breath turbulence, which starts the oscillation and is true of the real
const float kBrassNoiseGain = 0.02f;

// Nonlinear wave propagation. In a real bore the compression phase of a
const float kBrassSteepening = 112.0f;

// The steepening can never displace the read pointer by more than this fraction
const float kBrassSteepeningLimit = 0.25f;

class Brass {
 public:
  Brass() { }
  ~Brass() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  // Pattern A: the flow at the bell against the pressure in the mouthpiece.

 private:
  float* delay_line_;
  int delay_write_;

  // Lip: an outward-striking mass-spring-damper valve.
  float lip_x_;
  float lip_v_;

  float bell_lp_;
  float prev_arriving_;
  float junction_;
  float dc_x1_;
  float dc_y1_;
  float out_dc_x1_;
  float out_dc_y1_;
  float aux_dc_x1_;
  float aux_dc_y1_;

  DISALLOW_COPY_AND_ASSIGN(Brass);
};

class BrassEngine : public Engine {
 public:
  BrassEngine() { }
  ~BrassEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    brass_.Init(allocator);
  }

  virtual void Reset() {
    brass_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    brass_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Brass brass_;

  DISALLOW_COPY_AND_ASSIGN(BrassEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BRASS_ENGINE_H_

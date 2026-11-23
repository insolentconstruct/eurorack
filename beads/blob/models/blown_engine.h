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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Blown: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_BLOWN_ENGINE_H_
#define BEADS_BLOB_MODELS_BLOWN_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// Braids' kWGBoreLength (digital_oscillator.h:43). Keeping it keeps the
const size_t kBlownBoreLength = 2048;

// digital_oscillator.cc:1298-1301, normalized by the fixed-point scale each
const float kBlownBreathPressure = 26214.0f / 32768.0f;      // 0.80000
const float kBlownReflection = -3891.0f / 4096.0f;           // -0.94995
const float kBlownReedSlope = -1229.0f / 4096.0f;            // -0.30005
const float kBlownReedOffset = 22938.0f / 32768.0f;          // 0.69995

// Braids' CLIP() macro pins to +/-32767, not +/-32768 (stmlib/stmlib.h:39).
const float kBlownClip = 32767.0f / 32768.0f;

// The two added macros are bounded by the model's OSCILLATION WINDOW, which is
const float kBlownBlowMin = 0.55f;
const float kBlownBlowMax = 1.00f;

const float kBlownReedMin = 0.50f;
const float kBlownReedMax = 0.76f;

// Second pickup for the stereo render, as a fraction of the bore delay. Far
const float kBlownStereoPickup = 0.62f;

// The mono AUX is the raw bore pressure at unity -- no trim and no make-up.
const float kBlownAuxTrim = 1.0f;

// One-pole DC blocker, ~7.6 Hz at 48 kHz. Far below anything the bore makes.
const float kBlownDcCoefficient = 0.001f;

class Blown {
 public:
  Blown() { }
  ~Blown() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float* bore_;
  uint32_t delay_pointer_;

  // The loop's half-sample averager (digital_oscillator.cc:1344-1345).
  float lp_state_;
  // The output body filter (1355-1356).
  float body_state_;
  float body_state_aux_;

  float noise_depth_;
  float body_coefficient_;
  float blow_;
  float reed_offset_;

  uint32_t rng_state_;

  // 96 -> 48 kHz halfband history, one per output channel.
  float history_[32];
  float history_aux_[32];
  uint32_t history_write_;

  float out_dc_input_;
  float out_dc_output_;
  float aux_dc_input_;
  float aux_dc_output_;

  DISALLOW_COPY_AND_ASSIGN(Blown);
};

class BlownEngine : public Engine {
 public:
  BlownEngine() { }
  ~BlownEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    blown_.Init(allocator);
  }

  virtual void Reset() {
    blown_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    blown_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Blown blown_;

  DISALLOW_COPY_AND_ASSIGN(BlownEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BLOWN_ENGINE_H_

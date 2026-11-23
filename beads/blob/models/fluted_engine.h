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
// Fluted: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_FLUTED_ENGINE_H_
#define BEADS_BLOB_MODELS_FLUTED_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// Braids' kWGFBoreLength and kWGJetLength (digital_oscillator.h:44-45), and
const size_t kFlutedBoreLength = 4096;
const size_t kFlutedJetLength = 1024;

// digital_oscillator.cc:1365 -- kDCBlockingPole = uint16_t(0.99 * 4096).
const int32_t kFlutedDcBlockingPole = 4055;

// The excitation envelope's plateau index. Braids clamps excitation_ptr to
const int kFlutedEnvelopePlateau = 360;
const int kFlutedEnvelopeAttack = 120;
const int kFlutedEnvelopeDecay = 240;

// Blow: Braids welds the breath at 2x the excitation envelope
const float kFlutedBlowStock = 1.0f;
const float kFlutedBlowMin = 0.90f;
const float kFlutedBlowMax = 1.30f;

// Body: an offset, in semitones, on the note index Braids uses to pick the
const float kFlutedBodyStock = 0.0f;
const float kFlutedBodyMin = -12.0f;
const float kFlutedBodyMax = 36.0f;

// The second bore tap AUX reads, as a fraction of the bore delay. The
const float kFlutedAuxPickup = 0.5f;

// One-pole DC blocker, ~7.6 Hz at 48 kHz.
const float kFlutedDcCoefficient = 0.001f;

class Fluted {
 public:
  Fluted() { }
  ~Fluted() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  int8_t* bore_;
  int8_t* jet_;
  uint32_t delay_pointer_;

  // The reflection one-pole (digital_oscillator.cc:1425-1426).
  int32_t lp_state_;
  // The in-loop DC blocker (1428-1431).
  int32_t dc_blocking_x0_;
  int32_t dc_blocking_y0_;

  // The excitation envelope pointer, and the divider that reproduces Braids'
  int excitation_pointer_;
  // Exact BlowingEnvelope(excitation_pointer_), cached because the pointer
  int blowing_envelope_;
  uint32_t excitation_divider_;

  float noise_depth_;
  float blow_;

  uint32_t rng_state_;

  // 96 -> 48 kHz halfband history, one per output channel.
  float history_[32];
  float history_aux_[32];
  uint32_t history_write_;

  float out_dc_input_;
  float out_dc_output_;
  float aux_dc_input_;
  float aux_dc_output_;

  DISALLOW_COPY_AND_ASSIGN(Fluted);
};

class FlutedEngine : public Engine {
 public:
  FlutedEngine() { }
  ~FlutedEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    fluted_.Init(allocator);
  }

  virtual void Reset() {
    fluted_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    fluted_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Fluted fluted_;

  DISALLOW_COPY_AND_ASSIGN(FlutedEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_FLUTED_ENGINE_H_

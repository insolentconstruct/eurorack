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
// Ring Mod: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_RING_MOD_ENGINE_H_
#define BEADS_BLOB_MODELS_RING_MOD_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kRingModDetuneRange = 32.0f;

const float kRingModMaxPitch = 128.0f;

const float kRingModPreShaperScale = 0.25f;

const float kRingModShaperNorm = 1.016129f;

const float kRingModAuxGain = 0.51f;

const float kRingModHalfbandCentre = 0.500547367f;
const float kRingModHalfband1 = 0.310005574f;
const float kRingModHalfband3 = -0.082226011f;
const float kRingModHalfband5 = 0.029547365f;
const float kRingModHalfband7 = -0.007600612f;

class RingMod {
 public:
  RingMod() { }
  ~RingMod() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  inline float Decimate(const float* history) const;

  float phase_;
  float modulator_phase_;
  float modulator_phase_2_;

  float history_[16];
  float history_aux_[16];
  int history_position_;

  DISALLOW_COPY_AND_ASSIGN(RingMod);
};

class RingModEngine : public Engine {
 public:
  RingModEngine() { }
  ~RingModEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    ring_mod_.Init(allocator);
  }

  virtual void Reset() {
    ring_mod_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    ring_mod_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  RingMod ring_mod_;

  DISALLOW_COPY_AND_ASSIGN(RingModEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_RING_MOD_ENGINE_H_

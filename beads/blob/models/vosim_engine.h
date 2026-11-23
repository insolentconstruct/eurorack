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
// Vosim: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_VOSIM_ENGINE_H_
#define BEADS_BLOB_MODELS_VOSIM_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kVosimPedestal = 24576.0f / 32768.0f;
const float kVosimStockShare = 16384.0f / 24576.0f;

const float kVosimSineOffset = 127.0f / 32768.0f;
const float kVosimSineAmplitude = 32639.0f / 32768.0f;

const float kVosimStockBreakpoint = 16.0f / 256.0f;
const float kVosimMinBreakpoint = 1.0f / 256.0f;
const float kVosimMaxBreakpoint = 0.5f;

const int kVosimSeedSteps = 1024;
const int kVosimSeedStepsPerBlock = 64;

const float kVosimSeedMaxRatio = 256.0f;

const float kVosimHalfbandCentre = 0.500547367f;
const float kVosimHalfband1 = 0.310005574f;
const float kVosimHalfband3 = -0.082226011f;
const float kVosimHalfband5 = 0.029547365f;
const float kVosimHalfband7 = -0.007600612f;

class Vosim {
 public:
  Vosim() { }
  ~Vosim() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void Prepare(const EngineParameters& parameters);
  inline float Decimate(const float* history) const;

  float phase_;

  float formant_phase_[2];

  float history_[16];
  float history_aux_[16];
  int history_position_;

  float dc_input_;
  float dc_input_aux_;

  bool dc_seeded_;
  int seed_step_;
  EngineParameters seed_parameters_;
  float seed_phase_[2];
  float sum_window_;
  float sum_formant_[2];

  DISALLOW_COPY_AND_ASSIGN(Vosim);
};

class VosimEngine : public Engine {
 public:
  VosimEngine() { }
  ~VosimEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    vosim_.Init(allocator);
  }

  virtual void Reset() {
    vosim_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    vosim_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Vosim vosim_;

  DISALLOW_COPY_AND_ASSIGN(VosimEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_VOSIM_ENGINE_H_

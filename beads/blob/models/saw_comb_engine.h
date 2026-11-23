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
// SawComb: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SAW_COMB_ENGINE_H_
#define BEADS_BLOB_MODELS_SAW_COMB_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"

namespace plaits {

const size_t kSawCombDelaySize = 4096;

const int32_t kSawCombPitchCeiling = 16384;

const int32_t kSawCombNoteFloor = 0;
const int32_t kSawCombNoteCeiling = 17920;

const float kSawCombWarpMean = -0.000004360736f;
const float kSawCombWarpScale = 1.037256100043f;
const float kSawCombWarpTop = 0.9921875f;

const float kSawCombShelfPole = 0.35f;
const float kSawCombShelfHf = 0.788f;

class SawComb {
 public:
  SawComb() { }
  ~SawComb() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  VariableShapeOscillator exciter_;
  int16_t* line_;
  uint32_t write_pointer_;
  int32_t filtered_pitch_;
  float loop_lp_;

  DISALLOW_COPY_AND_ASSIGN(SawComb);
};

class SawCombEngine : public Engine {
 public:
  SawCombEngine() { }
  ~SawCombEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    saw_comb_.Init(allocator);
  }

  virtual void Reset() {
    saw_comb_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    saw_comb_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  SawComb saw_comb_;

  DISALLOW_COPY_AND_ASSIGN(SawCombEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SAW_COMB_ENGINE_H_

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
// VowelFof: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_VOWEL_FOF_ENGINE_H_
#define BEADS_BLOB_MODELS_VOWEL_FOF_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/oscillator.h"

namespace plaits {

const int kVowelFofNumFormants = 5;
const int kVowelFofGridSize = 5;
const float kVowelFofDamping = 1.0f / 64.0f;
const float kVowelFofOutputScale = 0.125f;
const float kVowelFofExcitationScale = 0.5f;
const float kVowelFofSourceGain = 0.387f;
const float kVowelFofStereoMakeup = 2.21f;

class VowelFof {
 public:
  VowelFof() { }
  ~VowelFof() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  Oscillator excitation_;
  float svf_lp_[kVowelFofNumFormants];
  float svf_bp_[kVowelFofNumFormants];
  uint32_t noise_state_;

  DISALLOW_COPY_AND_ASSIGN(VowelFof);
};

class VowelFofEngine : public Engine {
 public:
  VowelFofEngine() { }
  ~VowelFofEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    vowel_fof_.Init(allocator);
  }

  virtual void Reset() {
    vowel_fof_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    vowel_fof_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  VowelFof vowel_fof_;

  DISALLOW_COPY_AND_ASSIGN(VowelFofEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_VOWEL_FOF_ENGINE_H_

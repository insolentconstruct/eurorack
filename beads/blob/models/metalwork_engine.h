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
// Metalwork: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_METALWORK_ENGINE_H_
#define BEADS_BLOB_MODELS_METALWORK_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kMetalworkNumModes = 6;

class Metalwork {
 public:
  Metalwork() { }
  ~Metalwork() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  int object_;
  float phase_a_[kMetalworkNumModes];
  float phase_b_[kMetalworkNumModes];
  float amplitude_[kMetalworkNumModes];
  float strike_envelope_;
  float strike_lowpass_;

  DISALLOW_COPY_AND_ASSIGN(Metalwork);
};

class MetalworkEngine : public Engine {
 public:
  MetalworkEngine() { }
  ~MetalworkEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    metalwork_.Init(allocator);
  }

  virtual void Reset() {
    metalwork_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    metalwork_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Metalwork metalwork_;

  DISALLOW_COPY_AND_ASSIGN(MetalworkEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_METALWORK_ENGINE_H_

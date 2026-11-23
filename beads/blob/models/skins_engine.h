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
// Skins: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SKINS_ENGINE_H_
#define BEADS_BLOB_MODELS_SKINS_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kSkinsNumModes = 8;

class Skins {
 public:
  Skins() { }
  ~Skins() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_[kSkinsNumModes];
  float amplitude_[kSkinsNumModes];
  float bend_ratio_;
  float strike_envelope_;
  float strike_lowpass_;

  DISALLOW_COPY_AND_ASSIGN(Skins);
};

class SkinsEngine : public Engine {
 public:
  SkinsEngine() { }
  ~SkinsEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    skins_.Init(allocator);
  }

  virtual void Reset() {
    skins_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    skins_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Skins skins_;

  DISALLOW_COPY_AND_ASSIGN(SkinsEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SKINS_ENGINE_H_

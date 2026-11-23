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
// Rulefield: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_RULEFIELD_ENGINE_H_
#define BEADS_BLOB_MODELS_RULEFIELD_ENGINE_H_

#include <stdint.h>

#include "plaits/dsp/engine/engine.h"

namespace plaits {
class Rulefield {
 public:
  Rulefield() { }
  ~Rulefield() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  static uint32_t RotateLeft(uint32_t value);
  static uint32_t RotateRight(uint32_t value);
  static float Density(uint32_t value);

  void Seed();
  void Evolve();
  void UpdateReadouts();
  float ReadWave(uint32_t row, float density, float phase, float shape) const;

  uint32_t row_;
  uint32_t previous_row_;
  uint32_t edge_row_;
  uint32_t activity_row_;
  uint32_t generation_;
  uint8_t rule_;
  float row_density_;
  float edge_density_;
  float activity_density_;
  float phase_;
  float evolution_phase_;

  DISALLOW_COPY_AND_ASSIGN(Rulefield);
};

class RulefieldEngine : public Engine {
 public:
  RulefieldEngine() { }
  ~RulefieldEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    rulefield_.Init(allocator);
  }

  virtual void Reset() {
    rulefield_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    rulefield_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Rulefield rulefield_;

  DISALLOW_COPY_AND_ASSIGN(RulefieldEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_RULEFIELD_ENGINE_H_

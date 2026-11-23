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
// Scanned: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SCANNED_ENGINE_H_
#define BEADS_BLOB_MODELS_SCANNED_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kScannedMasses = 32;
// ReadPickup wraps mass indices with a mask.
STATIC_ASSERT((kScannedMasses & (kScannedMasses - 1)) == 0,
              scanned_masses_must_be_a_power_of_two);

class Scanned {
 public:
  Scanned() { }
  ~Scanned() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  // Keep strike-time loops compact.
#if defined(__clang__)
  void Excite(float position, float width, float amount);
#else
  __attribute__((optimize("no-unroll-loops")))
  void Excite(float position, float width, float amount);
#endif
  void Step(
      float inharmonicity,
      float structure,
      float damping,
      float nonlinearity,
      bool driven,
      int drive_index);
  float ReadPickup(
      float phase,
      float timbre,
      float fold_amount,
      float* derivative) const;

  float position_[kScannedMasses];
  float velocity_[kScannedMasses];
  // Cache inverse masses until inharmonicity or structure changes.
  float inverse_mass_[kScannedMasses];
  float mass_inharmonicity_;
  float mass_structure_;
  float scan_phase_;
  float physics_phase_;
  bool reset_pending_;

  DISALLOW_COPY_AND_ASSIGN(Scanned);
};

class ScannedEngine : public Engine {
 public:
  ScannedEngine() { }
  ~ScannedEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    scanned_.Init(allocator);
  }

  virtual void Reset() {
    scanned_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    scanned_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Scanned scanned_;

  DISALLOW_COPY_AND_ASSIGN(ScannedEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SCANNED_ENGINE_H_

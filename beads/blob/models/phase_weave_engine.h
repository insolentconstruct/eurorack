// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
//
// Phase-cancellation oscillator bank.
//
// From Plaits Palette, ported to Mutable Instruments' engine interface: mono,
// no FM input, and Palette's fourth macro fixed at its neutral 0.5.

#ifndef BEADS_BLOB_MODELS_PHASE_WEAVE_ENGINE_H_
#define BEADS_BLOB_MODELS_PHASE_WEAVE_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// The DSP, not polymorphic, so that a blob can construct it in place.
class PhaseWeave {
 public:
  PhaseWeave() { }
  ~PhaseWeave() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_;
  float constellation_;
  float spread_;
  float cancellation_;
  float rotation_;

  DISALLOW_COPY_AND_ASSIGN(PhaseWeave);
};

class PhaseWeaveEngine : public Engine {
 public:
  PhaseWeaveEngine() { }
  ~PhaseWeaveEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    phase_weave_.Init(allocator);
  }

  virtual void Reset() {
    phase_weave_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    phase_weave_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  PhaseWeave phase_weave_;

  DISALLOW_COPY_AND_ASSIGN(PhaseWeaveEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_PHASE_WEAVE_ENGINE_H_

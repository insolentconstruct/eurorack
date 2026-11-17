// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
//
// Phase-cancellation oscillator bank.
//
// From Plaits Palette, ported to Mutable Instruments' engine interface: mono,
// no FM input, and Palette's fourth macro fixed at its neutral 0.5.

#ifndef BEADS_BLOB_PHASE_WEAVE_ENGINE_H_
#define BEADS_BLOB_PHASE_WEAVE_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

class PhaseWeaveEngine : public Engine {
 public:
  PhaseWeaveEngine() { }
  ~PhaseWeaveEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
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

  DISALLOW_COPY_AND_ASSIGN(PhaseWeaveEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_PHASE_WEAVE_ENGINE_H_

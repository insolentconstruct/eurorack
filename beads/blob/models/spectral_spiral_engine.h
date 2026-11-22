// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
//
// Complex frequency-shift feedback synthesis engine.
//
// From Plaits Palette (0390557b), ported to Mutable Instruments' engine
// interface as Phase Weave was: mono, no FM input, and Palette's fourth macro
// fixed at its neutral 0.5.
//
// OUT: in-phase (I) output of the complex frequency-shift feedback loop.
// AUX: quadrature (Q) projection of a counter-rotated return.

#ifndef BEADS_BLOB_MODELS_SPECTRAL_SPIRAL_ENGINE_H_
#define BEADS_BLOB_MODELS_SPECTRAL_SPIRAL_ENGINE_H_

#include "stmlib/utils/buffer_allocator.h"

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kSpectralSpiralDelay = 32;

class SpectralSpiral {
 public:
  SpectralSpiral() { }
  ~SpectralSpiral() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void ClearLoop();

  float delay_i_[kSpectralSpiralDelay];
  float delay_q_[kSpectralSpiralDelay];
  int write_index_;
  float source_phase_;
  float shift_phase_;
  bool reset_pending_;

  DISALLOW_COPY_AND_ASSIGN(SpectralSpiral);
};

class SpectralSpiralEngine : public Engine {
 public:
  SpectralSpiralEngine() { }
  ~SpectralSpiralEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    spectral_spiral_.Init(allocator);
  }

  virtual void Reset() {
    spectral_spiral_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    spectral_spiral_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  SpectralSpiral spectral_spiral_;

  DISALLOW_COPY_AND_ASSIGN(SpectralSpiralEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SPECTRAL_SPIRAL_ENGINE_H_

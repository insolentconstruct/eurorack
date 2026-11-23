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
// Helix: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_HELIX_ENGINE_H_
#define BEADS_BLOB_MODELS_HELIX_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "beads/blob/models/palette_chord_bank.h"

namespace plaits {

const int kHelixOctaves = 4;  // 16 voices -- the ship config. QEMU-estimated 80%
                              // of budget, hardware-verified clean after the

class Helix {
 public:
  Helix() { }
  ~Helix() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);
  bool stereo_capable() const { return true; }  // OUT/AUX render as a true L/R pair


 private:
  PaletteChordBank chords_;
  // Each voice is a rotating unit vector rather than a phase into a table: see
  float osc_x_[kHelixOctaves * kPaletteChordNumNotes];
  float osc_y_[kHelixOctaves * kPaletteChordNumNotes];
  // Per-voice coefficients, refreshed HALF the palette per block (staggered,
  float gain_[kHelixOctaves * kPaletteChordNumNotes];
  float cos_w_[kHelixOctaves * kPaletteChordNumNotes];
  float sin_w_[kHelixOctaves * kPaletteChordNumNotes];
  float pan_l_[kHelixOctaves * kPaletteChordNumNotes];
  float pan_r_[kHelixOctaves * kPaletteChordNumNotes];
  float inv_w_;
  // Staggered refresh: half the palette's coefficients per block, so every
  int setup_phase_;
  float weight_half_[2];
  float shift_;                                  // continuous octave glide, [0,1)
  DISALLOW_COPY_AND_ASSIGN(Helix);
};

class HelixEngine : public Engine {
 public:
  HelixEngine() { }
  ~HelixEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    helix_.Init(allocator);
  }

  virtual void Reset() {
    helix_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    helix_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Helix helix_;

  DISALLOW_COPY_AND_ASSIGN(HelixEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_HELIX_ENGINE_H_

// Copyright 2026 Combust.
// SPDX-License-Identifier: MIT
// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Palette port: mono, no FM input, fourth macro fixed at 0.5.

#ifndef BEADS_BLOB_MODELS_ZXPHASE48K_ENGINE_H_
#define BEADS_BLOB_MODELS_ZXPHASE48K_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

#include "beads/blob/models/zx_pulse_core.h"

namespace plaits {

class ZxPhase48k {
 public:
  ZxPhase48k() { }
  ~ZxPhase48k() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  pulse::FrameTick tick_;

  uint32_t phase1_;
  uint32_t phase2_;
  uint32_t ghost_phase_;
  uint16_t lfsr_;

  int zone_;             // hysteretic mix-method zone: 0 XOR, 1 OR, 2 AND
  int interval_;         // hysteretic MORPH interval-ladder step
  bool noise_mode_;      // hysteretic MORPH extreme: osc2 becomes an LFSR

  int duty2_index_;
  int sweep_direction_;
  uint32_t sweep_count_;

  float dc_main_;
  float dc_aux_;

  uint32_t feedback_bits_[16];
  uint32_t feedback_pos_;

  float kick_;

  DISALLOW_COPY_AND_ASSIGN(ZxPhase48k);
};

class ZxPhase48kEngine : public Engine {
 public:
  ZxPhase48kEngine() { }
  ~ZxPhase48kEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    zxphase48k_.Init(allocator);
  }

  virtual void Reset() {
    zxphase48k_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    zxphase48k_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  ZxPhase48k zxphase48k_;

  DISALLOW_COPY_AND_ASSIGN(ZxPhase48kEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_ZXPHASE48K_ENGINE_H_

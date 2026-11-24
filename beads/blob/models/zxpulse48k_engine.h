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

#ifndef BEADS_BLOB_MODELS_ZXPULSE48K_ENGINE_H_
#define BEADS_BLOB_MODELS_ZXPULSE48K_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "beads/blob/models/zxpulse48k_chord_bank.h"

#include "beads/blob/models/zx_pulse_core.h"

namespace plaits {

class ZxPulse48k {
 public:
  ZxPulse48k() { }
  ~ZxPulse48k() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  static const int kNumVoices = 5;
  static const int kHistorySize = 16;

  ZxPulseChordBank chords_;
  pulse::PinChannel channels_[kNumVoices];
  pulse::FrameTick tick_;

  bool arp_mode_;        // hysteretic MORPH halves: stack vs arp
  int stack_count_;      // hysteretic voice count in stack mode
  uint32_t tick_count_;
  int arp_index_;

  float dt_history_[kHistorySize];
  float width_history_[kHistorySize];
  int history_pos_;

  float pull_;           // smoothed pitch-pull amount, semitones

  DISALLOW_COPY_AND_ASSIGN(ZxPulse48k);
};

class ZxPulse48kEngine : public Engine {
 public:
  ZxPulse48kEngine() { }
  ~ZxPulse48kEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    zxpulse48k_.Init(allocator);
  }

  virtual void Reset() {
    zxpulse48k_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    zxpulse48k_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  ZxPulse48k zxpulse48k_;

  DISALLOW_COPY_AND_ASSIGN(ZxPulse48kEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_ZXPULSE48K_ENGINE_H_

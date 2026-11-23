// Copyright 2012 Emilie Gillet.
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
// DualSync: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_DUAL_SYNC_ENGINE_H_
#define BEADS_BLOB_MODELS_DUAL_SYNC_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// (parameter_[0] >> 2) at TIMBRE 1 is 8191, and Braids' pitch unit is 1/128
const float kDualSyncMaxInterval = 8191.0f / 128.0f;

// set_parameter(0) on a square is pw = (32768 - 0) << 16 = half of the phase
const float kDualSyncPulseWidth = 0.5f;

// (Mix(...) >> 2) * 3, macro_oscillator.cc:281.
const float kDualSyncOutputGain = 0.75f;

// Reset travels half a cycle either side of Braids' zero, which is as far as
const float kDualSyncMaxResetPhase = 0.5f;

// The two channels take opposing balance offsets in stereo. Small, because the
const float kDualSyncStereoBalance = 0.12f;

// 2x, to reach Braids' 96 kHz internal rate from 48 kHz (see RATE above).
const int kDualSyncOversampling = 2;

// The halfband decimator: 47 taps, held in a power-of-two ring so the window
const int kDualSyncHalfbandLength = 47;
const int kDualSyncHalfbandPairs = 12;
const int kDualSyncDecimatorSize = 128;

class DualSync {
 public:
  DualSync() { }
  ~DualSync() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);


 private:
  float master_phase_;
  float slave_phase_;
  bool master_high_;
  bool slave_high_;
  float master_next_sample_;
  float slave_next_sample_;

  float master_frequency_;
  float slave_frequency_;
  float balance_;
  float shape_;
  float reset_phase_;

  float master_history_[kDualSyncDecimatorSize];
  float slave_history_[kDualSyncDecimatorSize];
  int decimator_write_;

  DISALLOW_COPY_AND_ASSIGN(DualSync);
};

class DualSyncEngine : public Engine {
 public:
  DualSyncEngine() { }
  ~DualSyncEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    dual_sync_.Init(allocator);
  }

  virtual void Reset() {
    dual_sync_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    dual_sync_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  DualSync dual_sync_;

  DISALLOW_COPY_AND_ASSIGN(DualSyncEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_DUAL_SYNC_ENGINE_H_

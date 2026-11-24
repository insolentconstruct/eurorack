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
// Wave Scan: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_WAVE_SCAN_ENGINE_H_
#define BEADS_BLOB_MODELS_WAVE_SCAN_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "plaits/dsp/engine/engine.h"

namespace plaits {

enum WaveScanModel {
  WAVE_SCAN_MODEL_WAVETABLES,
  WAVE_SCAN_MODEL_MAP,
  WAVE_SCAN_MODEL_LINE,
  WAVE_SCAN_MODEL_LAST
};

const size_t kWaveScanWaveSize = 128;
const uint32_t kWaveScanWaveMask = 127;

const size_t kWaveScanNumBanks = 20;

const int32_t kWaveScanBankHysteresis = 64;

const int32_t kWaveScanMapCells = 15;

const int32_t kWaveScanLineLast = 63;

const size_t kWaveScanDecimatorSize = 32;
const size_t kWaveScanDecimatorStorageSize = 2 * kWaveScanDecimatorSize;
const uint32_t kWaveScanDecimatorMask = 31;

const float kWaveScanMaxIncrement = 0.0625f;

const float kWaveScanPhaseScale = 4294967296.0f;

const float kWaveScanStereoSpread = 0.10f;

class WaveScan {
 public:
  WaveScan() { }
  ~WaveScan() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void LoadUserData(const uint8_t* user_data) { }
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  inline void Push(uint32_t* write, float main, float side) {
    const uint32_t index = *write & kWaveScanDecimatorMask;
    decimator_main_[index] = main;
    decimator_main_[index + kWaveScanDecimatorSize] = main;
    decimator_aux_[index] = side;
    decimator_aux_[index + kWaveScanDecimatorSize] = side;
    ++*write;
  }

  uint32_t phase_;
  float frequency_;

  int32_t bank_parameter_;
  int32_t scan_;
  int32_t previous_scan_;

  stmlib::HysteresisQuantizer2 model_quantizer_;

  float decimator_main_[kWaveScanDecimatorStorageSize];
  float decimator_aux_[kWaveScanDecimatorStorageSize];
  uint32_t decimator_write_;

  DISALLOW_COPY_AND_ASSIGN(WaveScan);
};

class WaveScanEngine : public Engine {
 public:
  WaveScanEngine() { }
  ~WaveScanEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    wave_scan_.Init(allocator);
  }

  virtual void Reset() {
    wave_scan_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    wave_scan_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  WaveScan wave_scan_;

  DISALLOW_COPY_AND_ASSIGN(WaveScanEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_WAVE_SCAN_ENGINE_H_

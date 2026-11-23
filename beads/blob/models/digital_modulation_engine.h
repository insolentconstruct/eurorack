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
// Digital Modulation: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_DIGITAL_MODULATION_ENGINE_H_
#define BEADS_BLOB_MODELS_DIGITAL_MODULATION_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kDigitalModulationRadius = 23100.0f / 32768.0f;

const float kDigitalModulationStockFrame = 1088.0f;
const float kDigitalModulationMinFrame = 32.0f;
const float kDigitalModulationFrameSpan = 61.05f;
const float kDigitalModulationPreamble = 32.0f;
const float kDigitalModulationSyncA = 48.0f;
const float kDigitalModulationSyncB = 64.0f;

const float kDigitalModulationSymbolOffset = -12.0f;
const float kDigitalModulationSymbolRange = 32.0f;

const float kDigitalModulationDcPole = 0.999f;

class DigitalModulation {
 public:
  DigitalModulation() { }
  ~DigitalModulation() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_;
  float symbol_phase_;
  int32_t payload_filter_;
  int symbol_count_;
  uint8_t data_byte_;

  float shaped_i_;
  float shaped_q_;

  float dc_aux_in_;
  float dc_aux_out_;

  DISALLOW_COPY_AND_ASSIGN(DigitalModulation);
};

class DigitalModulationEngine : public Engine {
 public:
  DigitalModulationEngine() { }
  ~DigitalModulationEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    digital_modulation_.Init(allocator);
  }

  virtual void Reset() {
    digital_modulation_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    digital_modulation_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  DigitalModulation digital_modulation_;

  DISALLOW_COPY_AND_ASSIGN(DigitalModulationEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_DIGITAL_MODULATION_ENGINE_H_

// Copyright 2016 Emilie Gillet.
// Copyright 2026 Lyle Mills.
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

#ifndef BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VARIANT_ENGINE_H_
#define BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VARIANT_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"

#ifndef PLAITS_VA_VARIANT_TRAJECTORY_ON_TWIST
#define PLAITS_VA_VARIANT_TRAJECTORY_ON_TWIST 0
#endif

#ifndef PLAITS_VA_VARIANT_SPREAD_PRIMARY_RATIO
#define PLAITS_VA_VARIANT_SPREAD_PRIMARY_RATIO 0.5f
#endif

namespace plaits {

enum VariantRemedy {
  VARIANT_REMEDY_NONE = 0,
  VARIANT_REMEDY_LINEAR_FADE,
  VARIANT_REMEDY_SHAPE_SPREAD,
  VARIANT_REMEDY_NO_DRY,
  VARIANT_REMEDY_PULSE_WIDTH
};

class VirtualAnalogVariant {
 public:
  VirtualAnalogVariant()
      : remedy_(VARIANT_REMEDY_SHAPE_SPREAD),
        spread_primary_ratio_(PLAITS_VA_VARIANT_SPREAD_PRIMARY_RATIO) { }
  ~VirtualAnalogVariant() { }

  void set_remedy(VariantRemedy remedy) { remedy_ = remedy; }

  void set_spread_primary_ratio(float ratio) {
    spread_primary_ratio_ = ratio;
  }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float ComputeDetuning(float detune) const;

  VariableShapeOscillator primary_;
  VariableShapeOscillator auxiliary_;
  VariableShapeOscillator sync_;
  VariableShapeOscillator aux_sync_;

  float auxiliary_amount_;
  float xmod_amount_;
  float* temp_buffer_;
  float* secondary_buffer_;
  VariantRemedy remedy_;
  float spread_primary_ratio_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAnalogVariant);
};

class VirtualAnalogVariantEngine : public Engine {
 public:
  VirtualAnalogVariantEngine() { }
  ~VirtualAnalogVariantEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    virtual_analog_variant_.Init(allocator);
  }

  virtual void Reset() {
    virtual_analog_variant_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    virtual_analog_variant_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  VirtualAnalogVariant virtual_analog_variant_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAnalogVariantEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VARIANT_ENGINE_H_

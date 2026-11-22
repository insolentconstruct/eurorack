// Copyright 2021 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Plaits' VirtualAnalogVCFEngine as a plain class for a blob, its code
// unchanged.

#ifndef BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VCF_ENGINE_H_
#define BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VCF_ENGINE_H_

#include "stmlib/dsp/filter.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"
#include "plaits/dsp/oscillator/variable_saw_oscillator.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"

namespace plaits {

class VirtualAnalogVCF {
 public:
  VirtualAnalogVCF() { }
  ~VirtualAnalogVCF() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  stmlib::Svf svf_[2];
  VariableShapeOscillator oscillator_;
  VariableShapeOscillator sub_oscillator_;

  float previous_cutoff_;
  float previous_stage2_gain_;
  float previous_q_;
  float previous_gain_;
  float previous_sub_gain_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAnalogVCF);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_VIRTUAL_ANALOG_VCF_ENGINE_H_

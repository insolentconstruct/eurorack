// Copyright 2015 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Rings' modal resonator, rendered by Plaits' batched SVFs. Coefficients and
// pickup position are updated once per block.

#ifndef BEADS_DSP_MODAL_RESONATOR_H_
#define BEADS_DSP_MODAL_RESONATOR_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/limiter.h"
#include "plaits/dsp/physical_modelling/resonator.h"

namespace beads {

// A multiple of 8, sized so that all 32 grains still play beside it.
const int kNumResonatorModes = 24;
// Gain staging ahead of the mix: Rings' bank reaches +20 dBFS on noise.
const float kResonatorPeak = 0.5f;
// Scaled down, a sharp mode stays under the limiter's detector clamp.
const float kResonatorHeadroom = 1.0f / 16.0f;
// The default object: a harmonic, medium-bright string.
const float kResonatorStructure = 0.25f;
const float kResonatorBrightness = 0.5f;
const float kResonatorDamping = 0.5f;
const float kResonatorPosition = 0.3f;

struct ResonatorMaterial {
  float structure;
  float brightness;
};

const int kNumResonatorMaterials = 4;
const int kNumResonatorRingTimes = 4;

// The options page's curated values; Hygrostat can set exact ones instead.
extern const ResonatorMaterial kResonatorMaterials[kNumResonatorMaterials];
extern const float kResonatorRingTimes[kNumResonatorRingTimes];

class ModalResonator {
 public:
  ModalResonator() { }
  ~ModalResonator() { }

  void Init(float sample_rate);
  // Odd modes go to out, even modes to aux; aux == out gives the mono sum.
  void Process(const float* in, float* out, float* aux, size_t size);
  // Adds the limited stereo resonance to the frames.
  void Render(const float* in, FloatFrame* frames, size_t size);

  inline void set_frequency(float frequency) {
    frequency_ = frequency;
  }

  inline void set_structure(float structure) {
    structure_ = structure;
  }

  inline void set_brightness(float brightness) {
    brightness_ = brightness;
  }

  inline void set_damping(float damping) {
    damping_ = damping;
  }

  inline void set_position(float position) {
    position_ = position;
  }

  // The resonance's level in the mix; 1 is unity.
  inline void set_amount(float amount) {
    amount_ = amount;
  }

 private:
  float frequency_;
  float structure_;
  float brightness_;
  float damping_;
  float position_;
  float amount_;
  Limiter limiter_;

  plaits::ResonatorSvf<plaits::kModeBatchSize> odd_[kNumResonatorModes / 8];
  plaits::ResonatorSvf<plaits::kModeBatchSize> even_[kNumResonatorModes / 8];

  DISALLOW_COPY_AND_ASSIGN(ModalResonator);
};

}  // namespace beads

#endif  // BEADS_DSP_MODAL_RESONATOR_H_

// Copyright 2017 Emilie Gillet.
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
// The resonator in the processor: tuning, strikes and placement.

#include "beads/dsp/processor.h"

#include <algorithm>

namespace beads {

using namespace std;
using namespace stmlib;

void Processor::ProcessResonator(
    const RandomizedParameter& pitch,
    bool strike,
    FloatFrame* frames,
    size_t size) {
  const float sr = sample_rate();
  if (sr != resonator_sample_rate_) {
    resonator_.Init(sr);
    resonator_sample_rate_ = sr;
  }
  // PITCH and its CV, not its randomness; Recent notes plays the newest.
  float note = pitch.base;
  if (pitch.has_cv) {
    note += pitch.attenuverter >= 0.0f
        ? pitch.attenuverter * pitch.cv
        : NewestNote(0.0f);
  }
  const PitchScale scale = processor_digital_.pitch_scale();
  if (scale != PITCH_SCALE_OFF) {
    note = QuantizePitch(note, scale);
  }
  CONSTRAIN(note, -48.0f, 48.0f);
  resonator_.set_frequency(
      SemitonesToRatio(note + kResonatorRootNote - 69.0f) * 440.0f / sr);

  float excitation[size];
  for (size_t i = 0; i < size; ++i) {
    excitation[i] = (frames[i].l + frames[i].r) * kResonatorInputGain;
  }
  if (strike) {
    excitation[0] += kResonatorStrike;
  }
  resonator_.Render(excitation, frames, size);
}

bool Processor::AttackOnset() const {
  switch (media_) {
    case MEDIA_DIRTY_TAPE:
      return processor_dirty_tape_.attack_onset();
    case MEDIA_CLEAN_TAPE:
      return processor_clean_tape_.attack_onset();
    case MEDIA_VINTAGE_DIGITAL:
      return processor_vintage_digital_.attack_onset();
    default:
      return processor_digital_.attack_onset();
  }
}

}  // namespace beads

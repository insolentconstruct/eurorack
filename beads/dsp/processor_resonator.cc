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
    strike_filter_.Init();
    strike_sounding_ = false;
    resonator_sample_rate_ = sr;
  }
  if (resonator_controls_set_) {
    resonator_.set_structure(resonator_controls_.structure);
    resonator_.set_brightness(resonator_controls_.brightness);
    resonator_.set_damping(resonator_controls_.damping);
    resonator_.set_position(resonator_controls_.position);
    resonator_.set_amount(resonator_controls_.amount);
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
  const float frequency =
      SemitonesToRatio(note + kResonatorRootNote - 69.0f) * 440.0f / sr;
  resonator_.set_frequency(frequency);

  float excitation[size];
  for (size_t i = 0; i < size; ++i) {
    excitation[i] = (frames[i].l + frames[i].r) * kResonatorInputGain;
  }
  float strike_pulse = 0.0f;
  if (strike) {
    // Rings' strum: a pulse through a low-pass that follows brightness.
    const float brightness = resonator_.brightness();
    const float cutoff = min(frequency * SemitonesToRatio(
        (brightness * (2.0f - brightness) - 0.5f) * 96.0f), 0.499f);
    strike_filter_.set_f_q<FREQUENCY_DIRTY>(cutoff, 1.5f);
    strike_pulse = 0.25f * SemitonesToRatio(cutoff * cutoff * 24.0f) / cutoff;
    strike_sounding_ = true;
  }
  if (strike_sounding_) {
    float pulse[size];
    fill(&pulse[0], &pulse[size], 0.0f);
    pulse[0] = strike_pulse;
    strike_filter_.Process<FILTER_MODE_LOW_PASS>(pulse, pulse, size);
    float level = 0.0f;
    for (size_t i = 0; i < size; ++i) {
      excitation[i] += pulse[i];
      level = max(level, fabsf(pulse[i]));
    }
    strike_sounding_ = level > 1.0e-6f;
  }
  resonator_.Render(excitation, frames, size);
}

bool Processor::SeedStrike(
    SeedButtonState seed_button_state,
    const GateFlags* seed_input,
    size_t size) {
  bool strike = false;
  if (seed_button_state >= SEED_BUTTON_STATE_LOCKED) {
    // Latched, each edge at the jack strikes it.
    if (seed_input) {
      for (size_t i = 0; i < size; ++i) {
        strike = strike || (seed_input[i] & GATE_FLAG_RISING);
      }
    }
    seed_gate_ = true;
  } else {
    // Unlatched, the button and the jack make one gate.
    bool gate = seed_gate_;
    for (size_t i = 0; i < size; ++i) {
      const bool high = seed_button_state == SEED_BUTTON_STATE_ON ||
          (seed_input && (seed_input[i] & GATE_FLAG_HIGH));
      strike = strike || (high && !gate);
      gate = high;
    }
    seed_gate_ = gate;
  }
  return strike;
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

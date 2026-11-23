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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Plucked: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/plucked_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float ApplyMacro(float stock_value, float minimum_value,
    float maximum_value, float macro) {
  const float amount = macro * 2.0f;
  return macro < 0.5f
      ? minimum_value + (stock_value - minimum_value) * amount
      : stock_value + (maximum_value - stock_value) * (amount - 1.0f);
}

}  // namespace

const float kPluckedPan[kNumPluckVoices] = { 0.15f, 0.5f, 0.85f };

namespace {

// The struck period for one voice, clamped to what the fixed-size delay
inline float PluckedPeriod(float frequency) {
  float period = 1.0f / max(frequency, 1e-7f);
  CONSTRAIN(period, kPluckedMinPeriod, kPluckedDelaySize - 4.0f);
  return period;
}

// Mirrors the shift search at digital_oscillator.cc:1099-1105: the smallest
inline float PluckedQuantizedSize(float frequency) {
  float doubled_increment = frequency * 4294967296.0f;
  float size = 1024.0f;
  while (doubled_increment > 8388608.0f) {
    doubled_increment *= 0.5f;
    size *= 0.5f;
  }
  CONSTRAIN(size, 8.0f, static_cast<float>(kPluckedDelaySize));
  return size;
}


}  // namespace

void Plucked::Init(BufferAllocator* allocator) {
  chords_.Init(allocator);
  for (int v = 0; v < kNumPluckVoices; ++v) {
    delay_line_[v].Init(allocator->Allocate<float>(kPluckedDelaySize));
  }
  Reset();
}

void Plucked::Reset() {
  for (int v = 0; v < kNumPluckVoices; ++v) {
    delay_line_[v].Reset();
    period_[v] = kPluckedDelaySize * 0.25f;
    min_period_[v] = kPluckedMinPeriod;
    excitation_remaining_[v] = 0.0f;
  }
  active_voice_ = 0;
  chords_.Reset();
  slot_count_ = 0;
  slot_ = -1;
  tone_counter_ = 0;
  active_ratio_ = 1.0f;
  ever_struck_ = false;
  loss_frac_ = 0.0f;
  update_probability_frac_ = 1.0f;
}

int Plucked::SelectChord(
    const EngineParameters& parameters, float* ratios) {
  // Slot 0 is unison; slots 1..N are the table's chords. The quantizer's
  const int slot_count =
      PaletteChordBank::table_size(0) + 1;
  if (slot_count != slot_count_) {
    slot_count_ = slot_count;
    slot_quantizer_.Init(slot_count, 0.075f, false);
  }
  const int slot = slot_quantizer_.Process(parameters.morph * 1.02f);
  if (slot != slot_) {
    slot_ = slot;
    tone_counter_ = 0;
  }

  if (slot == 0) {
    ratios[0] = 1.0f;
    return 1;
  }
  chords_.set_chord_index(slot - 1, 0);
  int count = chords_.num_notes();
  CONSTRAIN(count, 1, kPaletteChordNumNotes);
  // A one-tone chord ("Octave" in the stock table) would only repeat the
  if (count == 1 && chords_.ratio(kPaletteChordNumNotes - 1) > 1.01f) {
    ratios[0] = chords_.ratio(0);
    ratios[1] = chords_.ratio(kPaletteChordNumNotes - 1);
    return 2;
  }
  for (int i = 0; i < count; ++i) {
    ratios[i] = chords_.ratio(i);
  }
  return count;
}

void Plucked::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;


  // TIMBRE: Damping, two regimes split at TIMBRE 0.5
  const float frequency = NoteToFrequency(parameters.note);
  float loss_pitch_term = kPluckedLossPitchBase -
      frequency * kPluckedLossFreqScale;
  CONSTRAIN(loss_pitch_term, kPluckedLossPitchFloor, kPluckedLossPitchBase);

  const float p0 = parameters.timbre * kPluckedParamFull;
  float target_update_probability;
  float target_loss_raw;
  if (p0 < kPluckedTimbreHalf) {
    target_update_probability = kPluckedUpdateProbabilityFull;
    target_loss_raw = loss_pitch_term *
        (kPluckedTimbreHalf - p0) / kPluckedTimbreHalf;
  } else {
    target_update_probability = kPluckedUpdateProbabilityBase -
        p0 * kPluckedUpdateProbabilitySlope;
    target_loss_raw = 0.0f;
  }
  float target_loss_frac = target_loss_raw / kPluckedLossFull;
  float target_update_frac = target_update_probability /
      kPluckedProbabilityFull;
  CONSTRAIN(target_update_frac, 0.0f, 1.0f);

  // MACRO: Stretch. Scales the shortfall of the update fraction below 1.0,
  const float stretch_scale = ApplyMacro(
      1.0f, 0.0f, kPluckedMaxStretch, 0.5f);
  target_update_frac = 1.0f - (1.0f - target_update_frac) * stretch_scale;
  CONSTRAIN(target_update_frac, 0.0f, 1.0f);

  ParameterInterpolator loss_modulation(&loss_frac_, target_loss_frac, size);
  ParameterInterpolator update_modulation(
      &update_probability_frac_, target_update_frac, size);

  // MORPH: Chord. Fully CCW is unison, reproducing the module (and
  float chord[kPaletteChordNumNotes];
  const int chord_length = SelectChord(parameters, chord);

  // TRIGGER_UNPATCHED has no Braids equivalent (the module always strikes
  const bool unpatched = (parameters.trigger & TRIGGER_UNPATCHED) != 0;
  const bool rising_edge = (parameters.trigger & TRIGGER_RISING_EDGE) != 0;
  const bool self_strike = unpatched && !ever_struck_;

  if (rising_edge || self_strike) {
    active_voice_ = (active_voice_ + 1) % kNumPluckVoices;
    // Successive strikes play the chord's tones in turn, from its first;
    active_ratio_ = chord[tone_counter_ % chord_length];
    ++tone_counter_;

    const float struck_period = PluckedPeriod(frequency * active_ratio_);
    period_[active_voice_] = struck_period;
    min_period_[active_voice_] = struck_period * 0.5f;

    // COLOR: Pluck, read once at strike time
    const float p1 = parameters.harmonics * kPluckedParamFull;
    const float width = kPluckedWidthGain * p1;
    float excitation_fraction = (kPluckedWidthBase + width) /
        kPluckedWidthFull;
    CONSTRAIN(excitation_fraction, 0.0f, 1.0f);
    const float quantized_size = PluckedQuantizedSize(
        frequency * active_ratio_);
    excitation_remaining_[active_voice_] = max(
        1.0f, excitation_fraction * quantized_size);
    ever_struck_ = true;
  }

  // Continuous pitch tracking for the currently active voice only
  {
    float tracked = PluckedPeriod(frequency * active_ratio_);
    tracked = max(tracked, min_period_[active_voice_]);
    period_[active_voice_] = tracked;
  }


  for (size_t i = 0; i < size; ++i) {
    const float loss_frac = loss_modulation.Next();
    const float update_frac = update_modulation.Next();

    float sum_out = 0.0f;
    float sum_aux = 0.0f;

    for (int v = 0; v < kNumPluckVoices; ++v) {
      float sample;
      if (excitation_remaining_[v] > 0.0f) {
        // digital_oscillator.cc:1149-1151: 25% of the content about to be
        const float noise = static_cast<float>(Random::GetSample()) *
            (1.0f / 32768.0f);
        const float old = delay_line_[v].Read(period_[v]);
        const float excited = 0.25f * old + 0.75f * noise;
        delay_line_[v].Write(excited);
        sample = excited;
        excitation_remaining_[v] -= 1.0f;
        sum_aux += excited;
      } else {
        const float period = period_[v];
        const float a = delay_line_[v].ReadHermite(period);
        const float b = delay_line_[v].ReadHermite(period - 1.0f);
        const float filtered = 0.5f * (a + b) * (1.0f - loss_frac);
        const float gate = Random::GetFloat();
        const float value = (gate < update_frac) ? filtered : a;
        delay_line_[v].Write(value);
        sample = value;
      }
      sum_out += sample;
    }

    CONSTRAIN(sum_out, -1.0f, 1.0f);
    CONSTRAIN(sum_aux, -1.0f, 1.0f);
    out[i] = sum_out;
    aux[i] = sum_aux;

  }
}

}  // namespace plaits

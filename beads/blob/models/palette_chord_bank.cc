// Copyright 2021 Emilie Gillet.
// Copyright 2026 Insolent Construct.
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
// Chords: wavetable and divide-down organ/string machine.

#include "beads/blob/models/palette_chord_bank.h"

#include "stmlib/dsp/units.h"
#define PLAITS_CHORD_TABLE_COUNT 3

namespace plaits {

using namespace stmlib;

#ifndef PLAITS_CHORD_CENTS
#define PLAITS_CHORD_COUNT 46
#define PLAITS_CHORD_TABLE_OFFSETS { 0, 11, 28 }
#define PLAITS_CHORD_TABLE_SIZES { 11, 17, 18 }
#define PLAITS_CHORD_CENTS { \
  { 0, 1, 1199, 1200 }, { 0, 700, 701, 1200 }, \
  { 0, 500, 700, 1200 }, { 0, 300, 700, 1200 }, \
  { 0, 300, 700, 1000 }, { 0, 300, 1000, 1400 }, \
  { 0, 300, 1000, 1700 }, { 0, 200, 900, 1600 }, \
  { 0, 400, 1100, 1400 }, { 0, 400, 700, 1100 }, \
  { 0, 400, 700, 1200 }, \
  { 0, 1, 1199, 1200 }, { 0, 700, 701, 1200 }, \
  { 0, 300, 700, 1200 }, { 0, 300, 700, 1000 }, \
  { 0, 300, 1000, 1400 }, { 0, 300, 1000, 1700 }, \
  { 0, 400, 700, 1200 }, { 0, 400, 700, 1100 }, \
  { 0, 400, 1100, 1400 }, { 0, 500, 700, 1200 }, \
  { 0, 200, 900, 1600 }, { 0, 400, 700, 900 }, \
  { 0, 700, 1600, 2300 }, { 0, 400, 700, 1000 }, \
  { 0, 700, 1000, 1300 }, { 0, 300, 600, 1000 }, \
  { 0, 300, 600, 900 }, \
  { 500, 1200, 1900, 2600 }, { 1400, 800, 1900, 2400 }, \
  { 1000, 1700, 1900, 2600 }, { 700, 1400, 2200, 2400 }, \
  { 1500, 1000, 1900, 2000 }, { 1200, 1900, 2000, 2700 }, \
  { 800, 1500, 2400, 2600 }, { 1700, 1200, 2000, 2600 }, \
  { 1400, 1700, 2000, 2300 }, { 1100, 1400, 1700, 2000 }, \
  { 700, 1400, 1700, 2300 }, { 400, 700, 1700, 2300 }, \
  { 1200, 700, 1600, 2300 }, { 900, 1200, 1600, 2300 }, \
  { 500, 1200, 1900, 2100 }, { 1400, 900, 1700, 2400 }, \
  { 1100, 500, 1900, 2400 }, { 700, 1400, 1700, 2400 } \
}
#define PLAITS_CHORD_ARP_LENGTHS { \
  1, 2, 3, 3, 4, 4, 4, 4, 4, 4, 3, \
  1, 2, 3, 4, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, \
  3, 4, 4, 4, 4, 3, 4, 3, 4, 4, 4, 4, 3, 3, 3, 4, 4, 4 \
}
#endif

static const uint8_t chord_table_offsets_[PLAITS_CHORD_TABLE_COUNT] =
    PLAITS_CHORD_TABLE_OFFSETS;
static const uint8_t chord_table_sizes_[PLAITS_CHORD_TABLE_COUNT] =
    PLAITS_CHORD_TABLE_SIZES;
static const int16_t chord_cents_[PLAITS_CHORD_COUNT][kPaletteChordNumNotes] =
    PLAITS_CHORD_CENTS;
static const uint8_t chord_arp_lengths_[PLAITS_CHORD_COUNT] =
    PLAITS_CHORD_ARP_LENGTHS;


#define PLAITS_CHORD_CENTS_REF chord_cents_
#define PLAITS_CHORD_ARP_LENGTHS_REF chord_arp_lengths_
#define PLAITS_CHORD_TABLE_OFFSETS_REF chord_table_offsets_
#define PLAITS_CHORD_TABLE_SIZES_REF chord_table_sizes_
#define PLAITS_CHORD_TABLE_COUNT_REF PLAITS_CHORD_TABLE_COUNT


void PaletteChordBank::Init(BufferAllocator* allocator) {
  (void) allocator;
  chord_set_option_ = 0xff;
  chord_index_ = -1;
  num_notes_ = 0;
}

void PaletteChordBank::UpdateRatios(int chord_index) {
  chord_index_ = chord_index;
  for (int i = 0; i < kPaletteChordNumNotes; ++i) {
    ratios_[i] = SemitonesToRatio(
        static_cast<float>(PLAITS_CHORD_CENTS_REF[chord_index][i]) * 0.01f);
  }
  num_notes_ = PLAITS_CHORD_ARP_LENGTHS_REF[chord_index];
}

void PaletteChordBank::set_chord(float parameter, uint8_t chord_set_option) {
  if (chord_set_option >= PLAITS_CHORD_TABLE_COUNT_REF) {
    chord_set_option = 0;
  }
  if (chord_set_option_ != chord_set_option) {
    chord_set_option_ = chord_set_option;
    chord_index_quantizer_.Init(
        PLAITS_CHORD_TABLE_SIZES_REF[chord_set_option_], 0.075f, false);
  }
  chord_index_quantizer_.Process(parameter * 1.02f);
  int chord_index = PLAITS_CHORD_TABLE_OFFSETS_REF[chord_set_option_] +
      chord_index_quantizer_.quantized_value();
  if (chord_index_ != chord_index) {
    UpdateRatios(chord_index);
  }
}

int PaletteChordBank::table_size(uint8_t chord_set_option) {
  if (chord_set_option >= PLAITS_CHORD_TABLE_COUNT_REF) {
    chord_set_option = 0;
  }
  return PLAITS_CHORD_TABLE_SIZES_REF[chord_set_option];
}

void PaletteChordBank::set_chord_index(int index, uint8_t chord_set_option) {
  if (chord_set_option >= PLAITS_CHORD_TABLE_COUNT_REF) {
    chord_set_option = 0;
  }
  const int size = PLAITS_CHORD_TABLE_SIZES_REF[chord_set_option];
  CONSTRAIN(index, 0, size - 1);
  const int chord_index = PLAITS_CHORD_TABLE_OFFSETS_REF[chord_set_option] +
      index;
  if (chord_index_ != chord_index) {
    UpdateRatios(chord_index);
  }
}

void PaletteChordBank::Reset() {
  chord_set_option_ = 0xff;
  chord_index_ = -1;
  set_chord(0.0f, 0);
  Sort();
}

int PaletteChordBank::ComputeChordInversion(
    float inversion,
    float* ratios,
    float* amplitudes) {
  const float* base_ratio = this->ratios();
  inversion = inversion * float(kPaletteChordNumNotes * kPaletteChordNumVoices);

  MAKE_INTEGRAL_FRACTIONAL(inversion);

  int num_rotations = inversion_integral / kPaletteChordNumNotes;
  int rotated_note = inversion_integral % kPaletteChordNumNotes;

  const float kBaseGain = 0.25f;

  int mask = 0;

  for (int i = 0; i < kPaletteChordNumNotes; ++i) {
    float transposition = 0.25f * static_cast<float>(
        1 << ((kPaletteChordNumNotes - 1 + inversion_integral - i) / kPaletteChordNumNotes));
    int target_voice = (i - num_rotations + kPaletteChordNumVoices) % kPaletteChordNumVoices;
    int previous_voice = (target_voice - 1 + kPaletteChordNumVoices) % kPaletteChordNumVoices;

    if (i == rotated_note) {
      ratios[target_voice] = base_ratio[i] * transposition;
      ratios[previous_voice] = ratios[target_voice] * 2.0f;
      amplitudes[previous_voice] = kBaseGain * inversion_fractional;
      amplitudes[target_voice] = kBaseGain * (1.0f - inversion_fractional);
    } else if (i < rotated_note) {
      ratios[previous_voice] = base_ratio[i] * transposition;
      amplitudes[previous_voice] = kBaseGain;
    } else {
      ratios[target_voice] = base_ratio[i] * transposition;
      amplitudes[target_voice] = kBaseGain;
    }

    if (i == 0) {
      if (i >= rotated_note) {
        mask |= 1 << target_voice;
      }
      if (i <= rotated_note) {
        mask |= 1 << previous_voice;
      }
    }
  }
  return mask;
}

}  // namespace plaits

// Copyright 2016 Emilie Gillet.
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
// Chord bank shared by several engines.

#ifndef BEADS_BLOB_MODELS_PALETTE_CHORD_BANK_H_
#define BEADS_BLOB_MODELS_PALETTE_CHORD_BANK_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "stmlib/utils/buffer_allocator.h"

#include <algorithm>

namespace plaits {

const int kPaletteChordNumNotes = 4;
const int kPaletteChordNumVoices = kPaletteChordNumNotes + 1;
// PaletteChordBank::Sort() is a four-input sorting network.
STATIC_ASSERT(kPaletteChordNumNotes == 4, chord_bank_sort_is_for_four_notes);

class PaletteChordBank {
 public:
  PaletteChordBank() { }
  ~PaletteChordBank() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();

  int ComputeChordInversion(
      float inversion, float* ratios, float* amplitudes);

  inline void Sort() {
    for (int i = 0; i < kPaletteChordNumNotes; ++i) {
      float r = ratios_[i];
      while (r > 2.0f) {
        r *= 0.5f;
      }
      sorted_ratios_[i] = r;
    }
    // Sort four ratios with a fixed comparison network.
    float* s = sorted_ratios_;
    CompareSwap(&s[0], &s[1]);
    CompareSwap(&s[2], &s[3]);
    CompareSwap(&s[0], &s[2]);
    CompareSwap(&s[1], &s[3]);
    CompareSwap(&s[1], &s[2]);
  }

  void set_chord(float parameter, uint8_t chord_set_option);

  // Number of chords in a table (an out-of-range option reads table 0, as
  // set_chord does).
  static int table_size(uint8_t chord_set_option);

  // Selects chord `index` of a table directly, for an engine that maps its
  // own control onto the table instead of using set_chord's quantizer.
  void set_chord_index(int index, uint8_t chord_set_option);



  inline int chord_index() const {
    return chord_index_;
  }

  inline const float* ratios() const {
    return ratios_;
  }

  inline float ratio(int note) const {
    return ratios_[note];
  }

  inline float sorted_ratio(int note) const {
    return sorted_ratios_[note];
  }

  inline int num_notes() const {
    return num_notes_;
  }

 private:
  static inline void CompareSwap(float* a, float* b) {
    if (*b < *a) {
      const float t = *a;
      *a = *b;
      *b = t;
    }
  }

  void UpdateRatios(int chord_index);

  stmlib::HysteresisQuantizer2 chord_index_quantizer_;
  uint8_t chord_set_option_;
  int chord_index_;
  int num_notes_;
  float ratios_[kPaletteChordNumNotes];
  float sorted_ratios_[kPaletteChordNumNotes];

  DISALLOW_COPY_AND_ASSIGN(PaletteChordBank);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_PALETTE_CHORD_BANK_H_

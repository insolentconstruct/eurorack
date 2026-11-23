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
// Palette's private chord bank for ZxPulse48k, using table 0.

#ifndef BEADS_BLOB_MODELS_ZXPULSE48K_CHORD_BANK_H_
#define BEADS_BLOB_MODELS_ZXPULSE48K_CHORD_BANK_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "stmlib/utils/buffer_allocator.h"

#include <algorithm>

namespace plaits {

const int kZxPulseChordNumNotes = 4;
const int kZxPulseChordNumVoices = kZxPulseChordNumNotes + 1;
STATIC_ASSERT(kZxPulseChordNumNotes == 4, chord_bank_sort_is_for_four_notes);

class ZxPulseChordBank {
 public:
  ZxPulseChordBank() { }
  ~ZxPulseChordBank() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();

  int ComputeChordInversion(
      float inversion, float* ratios, float* amplitudes);

  inline void Sort() {
    for (int i = 0; i < kZxPulseChordNumNotes; ++i) {
      float r = ratios_[i];
      while (r > 2.0f) {
        r *= 0.5f;
      }
      sorted_ratios_[i] = r;
    }
    float* s = sorted_ratios_;
    CompareSwap(&s[0], &s[1]);
    CompareSwap(&s[2], &s[3]);
    CompareSwap(&s[0], &s[2]);
    CompareSwap(&s[1], &s[3]);
    CompareSwap(&s[1], &s[2]);
  }

  void set_chord(float parameter, uint8_t chord_set_option);

  static int table_size(uint8_t chord_set_option);

  void set_chord_index(int index, uint8_t chord_set_option);

#ifdef PLAITS_CHORD_RUNTIME_TABLE
  static void SetRuntimeTables(
      const int16_t (*cents)[kZxPulseChordNumNotes],
      const uint8_t* arp_lengths,
      const uint8_t* table_offsets,
      const uint8_t* table_sizes,
      int table_count);
#endif  // PLAITS_CHORD_RUNTIME_TABLE

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
  float ratios_[kZxPulseChordNumNotes];
  float sorted_ratios_[kZxPulseChordNumNotes];

  DISALLOW_COPY_AND_ASSIGN(ZxPulseChordBank);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_ZXPULSE48K_CHORD_BANK_H_

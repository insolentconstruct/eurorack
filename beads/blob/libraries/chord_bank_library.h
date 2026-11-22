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
// Plaits' chord bank as a library: its ID, exports and their signatures. A
// dependent keeps its ChordBank in its own state and runs the library's code
// on it through the plaits_blob_import_chord_bank_* calls
// (chord_bank_imports.cc); the inline accessors stay in the dependent.

#ifndef BEADS_BLOB_LIBRARIES_CHORD_BANK_LIBRARY_H_
#define BEADS_BLOB_LIBRARIES_CHORD_BANK_LIBRARY_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/chords/chord_bank.h"

namespace beads {

// Permanent ID (profile-features.json).
const uint32_t kChordBankLibrary = 2;
// What ChordBank::Init takes from a dependent's arena.
const size_t kChordBankArenaSize =
    (plaits::kChordNumChords + 1) * plaits::kChordNumNotes * sizeof(float) +
    plaits::kChordNumChords * sizeof(int);

enum ChordBankLibraryExport {
  CHORD_BANK_INIT,
  CHORD_BANK_RESET,
  CHORD_BANK_COMPUTE_CHORD_INVERSION,
  CHORD_BANK_LIBRARY_EXPORT_LAST
};

// Each export takes the library first, then the chord bank it runs on and
// the method's own arguments.
typedef void ChordBankInit(
    const PlaitsBlobLibrary* library,
    plaits::ChordBank* chords,
    stmlib::BufferAllocator* allocator);
typedef void ChordBankReset(
    const PlaitsBlobLibrary* library,
    plaits::ChordBank* chords);
typedef int ChordBankComputeChordInversion(
    const PlaitsBlobLibrary* library,
    plaits::ChordBank* chords,
    float inversion,
    float* ratios,
    float* amplitudes);

}  // namespace beads

extern "C" {

// The library's exports.
beads::ChordBankInit chord_bank_init;
beads::ChordBankReset chord_bank_reset;
beads::ChordBankComputeChordInversion chord_bank_compute_chord_inversion;

// A dependent's calls to them.
beads::ChordBankInit plaits_blob_import_chord_bank_init;
beads::ChordBankReset plaits_blob_import_chord_bank_reset;
beads::ChordBankComputeChordInversion
    plaits_blob_import_chord_bank_compute_chord_inversion;

}  // extern "C"

#endif  // BEADS_BLOB_LIBRARIES_CHORD_BANK_LIBRARY_H_

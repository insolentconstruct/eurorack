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
// A dependent's calls into the chord bank library, in the shape the gate
// accepts.

#include "beads/blob/libraries/chord_bank_library.h"

using namespace beads;
using namespace plaits;
using namespace stmlib;

template<typename T>
inline T* Export(const PlaitsBlobLibrary* library, ChordBankLibraryExport e) {
  return reinterpret_cast<T*>(library->exports[e]);
}

void plaits_blob_import_chord_bank_init(
    const PlaitsBlobLibrary* library,
    ChordBank* chords,
    BufferAllocator* allocator) {
  Export<ChordBankInit>(library, CHORD_BANK_INIT)(library, chords, allocator);
}

void plaits_blob_import_chord_bank_reset(
    const PlaitsBlobLibrary* library,
    ChordBank* chords) {
  Export<ChordBankReset>(library, CHORD_BANK_RESET)(library, chords);
}

int plaits_blob_import_chord_bank_compute_chord_inversion(
    const PlaitsBlobLibrary* library,
    ChordBank* chords,
    float inversion,
    float* ratios,
    float* amplitudes) {
  return Export<ChordBankComputeChordInversion>(
      library, CHORD_BANK_COMPUTE_CHORD_INVERSION)(
          library, chords, inversion, ratios, amplitudes);
}

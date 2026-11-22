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
// Plaits' chord bank as a library, used by Chords, String machine and
// Chiptune: its exports run Plaits' ChordBank code unchanged. No init, no
// RAM.

#include "beads/blob/libraries/chord_bank_library.h"

using namespace beads;
using namespace plaits;
using namespace stmlib;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  kPlaitsBlobAbi2,
  kPlaitsBlobServicesVersion2,
  kPlaitsBlobKindLibrary,
  kChordBankLibrary,
  1,
  0,  // The packer fills the size, export table and GOT fields.
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  { 0, 0, 0 },
  { 0, 0, 0, 0 }
};

void chord_bank_init(
    const PlaitsBlobLibrary* library,
    ChordBank* chords,
    BufferAllocator* allocator) {
  chords->Init(allocator);
}

void chord_bank_reset(
    const PlaitsBlobLibrary* library,
    ChordBank* chords) {
  chords->Reset();
}

int chord_bank_compute_chord_inversion(
    const PlaitsBlobLibrary* library,
    ChordBank* chords,
    float inversion,
    float* ratios,
    float* amplitudes) {
  return chords->ComputeChordInversion(inversion, ratios, amplitudes);
}

// In ChordBankLibraryExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  reinterpret_cast<const void*>(&chord_bank_init),
  reinterpret_cast<const void*>(&chord_bank_reset),
  reinterpret_cast<const void*>(&chord_bank_compute_chord_inversion)
};

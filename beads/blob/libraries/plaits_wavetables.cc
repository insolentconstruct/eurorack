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
//
// -----------------------------------------------------------------------------
//
// Plaits' integrated wavetables as a data library, read by Wavetable, Chords,
// Wave terrain and Palette's wavetable models. No init, no RAM.

#include "beads/blob/plaits_blob.h"
#include "beads/blob/libraries/plaits_wavetables.h"
#include "plaits/resources.h"

using beads::PlaitsBlobLibraryHeader;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindLibrary,
  beads::kPlaitsWavetablesLibrary,
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

// In PlaitsWavetablesExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  plaits::wav_integrated_waves
};

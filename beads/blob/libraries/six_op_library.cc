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
// Plaits' 6-op FM banks and algorithms as a library. Its init unpacks the
// banks, as PlaitsVoice::UnpackFmBanks does for the compiled-in engine, and
// compiles the algorithms, as SixOpEngine::Init does for each engine.

#include <new>

#include "beads/blob/libraries/six_op_library.h"
#include "beads/blob/plaits_region.h"
#include "plaits/resources.h"

using namespace beads;
using namespace plaits;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  kPlaitsBlobAbi2,
  kPlaitsBlobServicesVersion2,
  kPlaitsBlobKindLibrary,
  kSixOpLibrary,
  1,
  0,  // The packer fills the size, init, export table and GOT fields.
  sizeof(SixOpLibrary),
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

STATIC_ASSERT(__alignof__(SixOpLibrary) <= kPlaitsLibraryRamAlignment,
    six_op_alignment);

static void UnpackBank(const uint8_t* data, fm::Patch* patches) {
  for (int i = 0; i < kSixOpBankSize; ++i) {
    patches[i].Unpack(data + i * fm::Patch::SYX_SIZE);
  }
}

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    uint8_t* scratch,
    size_t scratch_size,
    const PlaitsBlobServices* services,
    const PlaitsBlobLibrary* const* libraries) {
  SixOpLibrary* s = new(ram) SixOpLibrary;
  UnpackBank(syx_bank_0, &s->banks[0]);
  UnpackBank(syx_bank_1, &s->banks[kSixOpBankSize]);
  UnpackBank(syx_bank_2, &s->banks[2 * kSixOpBankSize]);
  s->algorithms.Init();
  return s;
}

// In SixOpLibraryExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  syx_bank_0,
  syx_bank_1,
  syx_bank_2
};

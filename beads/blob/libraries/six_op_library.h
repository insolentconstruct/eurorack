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
// Plaits' 6-op FM banks and algorithms as a library: its ID, RAM, exports and
// context. Its init unpacks the three banks and compiles the algorithms once
// at synth entry; the 6-op model reads both from the context and keeps its
// voices in its own state.

#ifndef BEADS_BLOB_LIBRARIES_SIX_OP_LIBRARY_H_
#define BEADS_BLOB_LIBRARIES_SIX_OP_LIBRARY_H_

#include "stmlib/stmlib.h"

#include "beads/blob/libraries/six_op_algorithms.h"
#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/fm/patch.h"

namespace beads {

// Permanent ID (profile-features.json).
const uint32_t kSixOpLibrary = 4;
const int kNumSixOpBanks = 3;
const int kSixOpBankSize = 32;

enum SixOpLibraryExport {
  SIX_OP_BANK_A,  // plaits::syx_bank_0, 32 packed patches
  SIX_OP_BANK_B,
  SIX_OP_BANK_C,
  SIX_OP_LIBRARY_EXPORT_LAST
};

// The context, at the start of the library's RAM.
struct SixOpLibrary {
  plaits::fm::Patch banks[kNumSixOpBanks * kSixOpBankSize];
  plaits::fm::IndexedAlgorithms<6> algorithms;
};

}  // namespace beads

#endif  // BEADS_BLOB_LIBRARIES_SIX_OP_LIBRARY_H_

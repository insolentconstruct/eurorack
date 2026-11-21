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
// Two local imports of one name: the first only branches through a register.
// Each is checked on its own (import_duplicate.S holds the second).

// Library: pos_library
// Source: beads/test/blob_gate/import_duplicate.S
// Expect: GATE: .*import plaits_blob_import_same is not one load

#define NUM_DEPENDENCIES 1

#include "beads/test/blob_gate/common2.h"

extern "C" void plaits_blob_call_same(const PlaitsBlobLibrary* library);

__asm__(
    ".text\n\t.thumb\n\t.p2align 2\n"
    "\t.type plaits_blob_import_same, %function\n"
    "plaits_blob_import_same:\n\tbx r3\n");

RENDER {
  plaits_blob_call_same(*static_cast<const PlaitsBlobLibrary* const*>(object));
  __asm__ volatile("bl plaits_blob_import_same");
}

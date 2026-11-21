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
// A code library: init keeps a gain in its RAM, its one export scales a
// block by it.

#include "beads/blob/plaits_blob.h"

using beads::PlaitsBlobLibrary;
using beads::PlaitsBlobLibraryHeader;
using beads::PlaitsBlobServices;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindLibrary,
  200, 1, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0,
  { 0, 0, 0 },
  { 0, 0, 0, 0 }
};

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    uint8_t* scratch,
    size_t scratch_size,
    const PlaitsBlobServices* services,
    const PlaitsBlobLibrary* const* libraries) {
  *reinterpret_cast<float*>(ram) = 0.5f;
  return ram;
}

extern "C" void test_scale(
    const PlaitsBlobLibrary* library,
    float* out,
    size_t size) {
  const float gain = *static_cast<const float*>(library->context);
  for (size_t i = 0; i < size; ++i) {
    out[i] *= gain;
  }
}

extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  reinterpret_cast<const void*>(&test_scale)
};

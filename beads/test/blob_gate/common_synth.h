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
// Blob gate controls for synth headers (kind 3); RAM_SIZE, ZERO_0,
// BLOCK_SIZE and NUM_DEPENDENCIES may be predefined.

#ifndef BEADS_TEST_BLOB_GATE_COMMON_SYNTH_H_
#define BEADS_TEST_BLOB_GATE_COMMON_SYNTH_H_

#include "beads/blob/plaits_blob.h"

using beads::PlaitsBlobServices;
using beads::PlaitsBlobSynthHeader;

#ifndef RAM_SIZE
#define RAM_SIZE 16
#endif  // RAM_SIZE

#ifndef ZERO_0
#define ZERO_0 0
#endif  // ZERO_0

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 12
#endif  // BLOCK_SIZE

#ifndef NUM_DEPENDENCIES
#define NUM_DEPENDENCIES 0
#endif  // NUM_DEPENDENCIES

extern "C" const PlaitsBlobSynthHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobSynthHeader),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindSynth,
  1, 1, 0, RAM_SIZE, ZERO_0, 0, 0, 0, 0, 0, 0, NUM_DEPENDENCIES, 0,
  36, BLOCK_SIZE, { 0, 0, 0, 0 }
};

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    const PlaitsBlobServices* services) {
  return ram;
}

extern "C" bool plaits_blob_configure(
    void* context,
    const void* target,
    const void* controls,
    bool entry) {
  return true;
}

extern "C" void plaits_blob_render(
    void* context,
    void* controls,
    float* out,
    float* aux,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    out[i] = aux[i] = 0.0f;
  }
}

#endif  // BEADS_TEST_BLOB_GATE_COMMON_SYNTH_H_

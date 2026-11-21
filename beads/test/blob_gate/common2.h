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
// Blob gate controls for version 2 headers; STATE_SIZE and NUM_DEPENDENCIES
// may be predefined.

#ifndef BEADS_TEST_BLOB_GATE_COMMON2_H_
#define BEADS_TEST_BLOB_GATE_COMMON2_H_

#include <cstring>

#include "beads/blob/plaits_blob.h"
#include "plaits/resources.h"

using beads::PlaitsBlobHeader2;
using beads::PlaitsBlobLibrary;
using beads::PlaitsBlobServices;
using plaits::EngineParameters;

#ifndef STATE_SIZE
#define STATE_SIZE 16
#endif  // STATE_SIZE

#ifndef NUM_DEPENDENCIES
#define NUM_DEPENDENCIES 0
#endif  // NUM_DEPENDENCIES

extern "C" const PlaitsBlobHeader2 plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobHeader2),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindModel,
  100, 1, 0, STATE_SIZE, 0, 0, 0, 0, 0, 0, 0, NUM_DEPENDENCIES, 0, 1.0f, 1.0f,
  { 0, 0, 0, 0 }
};

extern "C" void* plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size,
    const PlaitsBlobLibrary* const* libraries) {
  return state;
}

extern "C" void plaits_blob_reset(
    void* object,
    const PlaitsBlobServices* services,
    uint32_t argument) { }

#define RENDER extern "C" void plaits_blob_render( \
    void* object, const PlaitsBlobServices* services, \
    const EngineParameters* p, float* out, float* aux, size_t size, \
    bool* already_enveloped)

#endif  // BEADS_TEST_BLOB_GATE_COMMON2_H_

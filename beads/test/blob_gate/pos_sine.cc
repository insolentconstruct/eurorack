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
// A positive control: a second, well-formed blob that the gate must accept.

#include <new>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "beads/test/blob_gate/test_sine.h"

using beads::PlaitsBlobHeader;
using beads::PlaitsBlobServices;
using beads::TestSine;
using plaits::EngineParameters;

#ifndef TEST_SINE_MODEL
#define TEST_SINE_MODEL 37
#endif

extern "C" const PlaitsBlobHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobHeader),
  beads::kPlaitsBlobAbi,
  beads::kPlaitsBlobServicesVersion,
  TEST_SINE_MODEL,
  0,
  sizeof(TestSine),
  0, 0, 0, 0, 0, 0, 0, 1.0f, 1.0f
};

extern "C" void* plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size) {
  TestSine* sine = new(state) TestSine;
  sine->Init();
  return sine;
}

extern "C" void plaits_blob_reset(
    void* object,
    const PlaitsBlobServices* services) {
  static_cast<TestSine*>(object)->Init();
}

extern "C" void plaits_blob_render(
    void* object,
    const PlaitsBlobServices* services,
    const EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  static_cast<TestSine*>(object)->Render(*parameters, out, aux, size);
}

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
// Phase Weave as a blob. The engine is never constructed and has no vtable:
// the entry points call it non-virtually on the voice's state.

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/phase_weave_engine.h"

using beads::PlaitsBlobHeader;
using beads::PlaitsBlobServices;
using plaits::EngineParameters;
using plaits::PhaseWeaveEngine;

// Provisional catalogue ID; Palette's output gain.
const uint32_t kModel = 36;
const float kGain = 0.93f;

extern "C" const PlaitsBlobHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobHeader),
  beads::kPlaitsBlobAbi,
  beads::kPlaitsBlobServicesVersion,
  kModel,
  0,  // The packer fills the size, entry and GOT fields.
  sizeof(PhaseWeaveEngine),
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  kGain,
  kGain
};

extern "C" void plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size) {
  stmlib::BufferAllocator allocator(arena, arena_size);
  static_cast<PhaseWeaveEngine*>(state)->PhaseWeaveEngine::Init(&allocator);
}

extern "C" void plaits_blob_reset(
    void* state,
    const PlaitsBlobServices* services) {
  static_cast<PhaseWeaveEngine*>(state)->PhaseWeaveEngine::Reset();
}

extern "C" void plaits_blob_render(
    void* state,
    const PlaitsBlobServices* services,
    const EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  static_cast<PhaseWeaveEngine*>(state)->PhaseWeaveEngine::Render(
      *parameters, out, aux, size, already_enveloped);
}

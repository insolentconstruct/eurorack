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
// Phase Weave as a blob: init constructs its DSP object in the voice's state.

#include <new>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/phase_weave_engine.h"

using beads::PlaitsBlobHeader;
using beads::PlaitsBlobServices;
using plaits::EngineParameters;
using plaits::PhaseWeave;

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
  sizeof(PhaseWeave),
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

STATIC_ASSERT(sizeof(PhaseWeave) <= beads::kPlaitsBlobMaxStateSize, state);
STATIC_ASSERT(__alignof__(PhaseWeave) <= beads::kPlaitsBlobStateAlignment,
    state_alignment);

extern "C" void* plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size) {
  stmlib::BufferAllocator allocator(arena, arena_size);
  PhaseWeave* phase_weave = new(state) PhaseWeave;
  phase_weave->Init(&allocator);
  return phase_weave;
}

extern "C" void plaits_blob_reset(
    void* object,
    const PlaitsBlobServices* services) {
  static_cast<PhaseWeave*>(object)->Reset();
}

extern "C" void plaits_blob_render(
    void* object,
    const PlaitsBlobServices* services,
    const EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  static_cast<PhaseWeave*>(object)->Render(
      *parameters, out, aux, size, already_enveloped);
}

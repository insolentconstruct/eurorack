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
// Speech as a version 2 blob: its synths live in its state, their code and the
// decoded word banks in the speech library.

#include <new>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/libraries/speech_library.h"
#include "beads/blob/models/speech_engine.h"

using beads::PlaitsBlobHeader2;
using beads::PlaitsBlobLibrary;
using beads::PlaitsBlobServices;
using plaits::EngineParameters;
using plaits::Speech;

// Permanent ID (profile-features.json); Plaits' gains, a negative OUT gain
// meaning the limiter (beads/dsp/plaits_voice.cc).
const uint32_t kModel = 15;
const float kOutGain = -0.7f;
const float kAuxGain = 0.8f;
// A word bank's frames and boundaries, then two blocks of scratch.
const uint32_t kArenaSize = beads::kSpeechLibraryScratchSize +
    2 * plaits::kMaxBlockSize * sizeof(float);

extern "C" const PlaitsBlobHeader2 plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobHeader2),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindModel,
  kModel,
  1,
  0,  // The packer fills the size, entry, GOT and dependency list fields.
  sizeof(Speech),
  kArenaSize,
  0,
  0,
  0,
  0,
  0,
  0,
  1,  // The speech library.
  0,
  kOutGain,
  kAuxGain,
  { 0, 0, 0, 0 }
};

STATIC_ASSERT(sizeof(Speech) <= beads::kPlaitsBlobMaxStateSize2, state);
STATIC_ASSERT(__alignof__(Speech) <= beads::kPlaitsBlobStateAlignment,
    state_alignment);

extern "C" void* plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size,
    const PlaitsBlobLibrary* const* libraries) {
  stmlib::BufferAllocator allocator(arena, arena_size);
  Speech* speech = new(state) Speech;
  speech->Init(libraries[0], &allocator);
  return speech;
}

extern "C" void plaits_blob_reset(
    void* object,
    const PlaitsBlobServices* services,
    uint32_t argument) {
  static_cast<Speech*>(object)->Reset();
}

extern "C" void plaits_blob_render(
    void* object,
    const PlaitsBlobServices* services,
    const EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  static_cast<Speech*>(object)->Render(
      *parameters, out, aux, size, already_enveloped);
}

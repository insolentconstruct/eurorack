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
// 6-op FM banks A, B and C (models 2-4) as one version 2 blob: its banks and
// algorithms come from the 6-op library. reset loads the selected bank, as the
// voice does for the compiled-in engine, then its Reset.

#include <new>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/models/six_op_fm_engine.h"
#include "beads/blob/plaits_blob.h"

using beads::PlaitsBlobHeader2;
using beads::PlaitsBlobLibrary;
using beads::PlaitsBlobServices;
using plaits::EngineParameters;
using plaits::SixOpFM;

// Permanent ID (profile-features.json); Plaits' gains
// (beads/dsp/plaits_voice.cc).
const uint32_t kModel = 2;
const float kOutGain = 1.0f;
const float kAuxGain = 1.0f;
// Two buffers, then the bank LoadUserData would unpack.
const uint32_t kArenaSize =
    plaits::kMaxBlockSize * (4 + plaits::kNumSixOpVoices) * sizeof(float) +
    beads::kSixOpBankSize * sizeof(plaits::fm::Patch);

extern "C" const PlaitsBlobHeader2 plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobHeader2),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindModel,
  kModel,
  3,  // Banks A, B and C.
  0,  // The packer fills the size, entry, GOT and dependency list fields.
  sizeof(SixOpFM),
  kArenaSize,
  0,
  0,
  0,
  0,
  0,
  0,
  1,  // The 6-op library.
  beads::PLAITS_BLOB_FLAG_ENVELOPED,
  kOutGain,
  kAuxGain,
  { 0, 0, 0, 0 }
};

STATIC_ASSERT(sizeof(SixOpFM) <= beads::kPlaitsBlobMaxStateSize2, state);
STATIC_ASSERT(__alignof__(SixOpFM) <= beads::kPlaitsBlobStateAlignment,
    state_alignment);

extern "C" void* plaits_blob_init(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size,
    const PlaitsBlobLibrary* const* libraries) {
  stmlib::BufferAllocator allocator(arena, arena_size);
  SixOpFM* six_op_fm = new(state) SixOpFM;
  six_op_fm->Init(
      static_cast<const beads::SixOpLibrary*>(libraries[0]->context),
      &allocator);
  return six_op_fm;
}

extern "C" void plaits_blob_reset(
    void* object,
    const PlaitsBlobServices* services,
    uint32_t argument) {
  // The argument is the bank: models 2, 3 and 4.
  SixOpFM* six_op_fm = static_cast<SixOpFM*>(object);
  six_op_fm->LoadBank(argument);
  six_op_fm->Reset();
}

extern "C" void plaits_blob_render(
    void* object,
    const PlaitsBlobServices* services,
    const EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  static_cast<SixOpFM*>(object)->Render(
      *parameters, out, aux, size, already_enveloped);
}

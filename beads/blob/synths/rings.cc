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
// The Rings synth as a blob (kind 3): init builds the core in the RAM the app
// lends from delay bank 0, zeroed; configure and render drive it.

#include <new>

#include "beads/blob/plaits_blob.h"
#include "beads/blob/synths/rings_synth_core.h"

using beads::PlaitsBlobServices;
using beads::PlaitsBlobSynthHeader;
using beads::RingsSynthControls;
using beads::RingsSynthCore;
using beads::RingsSynthTarget;

extern "C" const PlaitsBlobSynthHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic,
  sizeof(PlaitsBlobSynthHeader),
  beads::kPlaitsBlobAbi2,
  beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindSynth,
  beads::kRingsSynthId,
  1,
  0,  // The packer fills the size, entry and GOT fields.
  sizeof(RingsSynthCore),
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  sizeof(RingsSynthControls),
  rings::kMaxBlockSize,
  { 0, 0, 0, 0 }
};

STATIC_ASSERT(sizeof(RingsSynthCore) <= beads::kPlaitsBlobMaxSynthRamSize,
    ram);
STATIC_ASSERT(__alignof__(RingsSynthCore) <= beads::kPlaitsBlobStateAlignment,
    ram_alignment);

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    const PlaitsBlobServices* services) {
  RingsSynthCore* core = new(ram) RingsSynthCore;
  core->Init();
  return core;
}

extern "C" bool plaits_blob_configure(
    void* context,
    const RingsSynthTarget* target,
    const RingsSynthControls* controls,
    bool entry) {
  return static_cast<RingsSynthCore*>(context)->Configure(
      *target, *controls, entry);
}

extern "C" void plaits_blob_render(
    void* context,
    RingsSynthControls* controls,
    float* out,
    float* aux,
    size_t size) {
  static_cast<RingsSynthCore*>(context)->Render(controls, out, aux, size);
}

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
// The Rings synth blob's interface: its control block, target and entries,
// without Rings' own headers (the app includes Beads' tables).

#ifndef BEADS_BLOB_SYNTHS_RINGS_SYNTH_CONTROLS_H_
#define BEADS_BLOB_SYNTHS_RINGS_SYNTH_CONTROLS_H_

#include "stmlib/stmlib.h"

#include "rings/dsp/patch.h"
#include "rings/dsp/performance_state.h"

namespace beads {

// Permanent synth ID of the Rings synth blob.
const uint32_t kRingsSynthId = 1;
// Beads' block: the blob must be built for it.
const size_t kRingsBlockSize = 12;
// Part's models, Disastrous peace's FX and its default (rings/dsp).
const int kNumRingsModels = 6;
const int kNumRingsFx = 6;
const int kRingsDefaultFx = 4;

enum RingsCoreType {
  RINGS_CORE_PART,
  RINGS_CORE_STRING_SYNTH,
  RINGS_CORE_LAST
};

// One block's controls, fixed by the blob's interface.
struct RingsSynthControls {
  rings::Patch patch;
  rings::PerformanceState performance_state;
};

// What plays: a core, Part's model (0-5), the polyphony (1, 2 or 4) and
// Disastrous peace's FX (0-5).
struct RingsSynthTarget {
  uint8_t core;
  uint8_t model;
  uint8_t polyphony;
  uint8_t fx;
};

struct PlaitsBlobServices;

// The blob's entries. Init and configure run in the main loop with the codec
// paused; render in the audio interrupt.
typedef void* (*RingsSynthInitFn)(
    uint8_t* ram,
    size_t ram_size,
    const PlaitsBlobServices* services);
typedef bool (*RingsSynthConfigureFn)(
    void* context,
    const RingsSynthTarget* target,
    const RingsSynthControls* controls,
    bool entry);
typedef void (*RingsSynthRenderFn)(
    void* context,
    RingsSynthControls* controls,
    float* out,
    float* aux,
    size_t size);

STATIC_ASSERT(sizeof(rings::Patch) == 16, rings_patch);
STATIC_ASSERT(sizeof(rings::PerformanceState) == 20, rings_performance_state);
STATIC_ASSERT(sizeof(RingsSynthControls) == 36, rings_synth_controls);
STATIC_ASSERT(sizeof(RingsSynthTarget) == 4, rings_synth_target);

}  // namespace beads

#endif  // BEADS_BLOB_SYNTHS_RINGS_SYNTH_CONTROLS_H_

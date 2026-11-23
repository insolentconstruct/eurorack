// Copyright 2012 Emilie Gillet.
// Copyright 2018 Tom Burns.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT

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
//
// ScaleStack: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/scale_stack_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void ScaleStack::Init(
    const beads::PlaitsBlobLibrary* scales,
    BufferAllocator* allocator) {
  scales_ = scales;
  num_scales_ = *static_cast<const int*>(
      scales_->exports[beads::SCALE_VOICES_NUM_SCALES]);
  (void) allocator;
  plaits_blob_import_scale_voices_init(scales_, &voices_);
}

void ScaleStack::Reset() {
  plaits_blob_import_scale_voices_reset(scales_, &voices_);
}

void ScaleStack::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    plaits_blob_import_scale_voices_reset(scales_, &voices_);
  }

  int scale = static_cast<int>(0.5f * num_scales_);
  CONSTRAIN(scale, 0, num_scales_ - 1);

  int span = 1 + static_cast<int>(
      parameters.harmonics * kBlobScaleStackMaxSpan);
  CONSTRAIN(span, 1, kBlobScaleStackMaxSpan);

  float residual = 0.0f;
  const int root_degree = plaits_blob_import_scale_voices_quantize(
      scales_, &voices_, parameters.note, scale, &residual);

  float notes[kScaleVoicesMaxVoices];
  notes[0] = parameters.note;
  for (int v = 1; v < kBlobScaleStackNumVoices; ++v) {
    int degree = v * span;
    CONSTRAIN(degree, 0, kScaleVoicesMaxDegreeOffset);
    notes[v] = plaits_blob_import_scale_voices_degree_to_note(
      scales_, root_degree + degree, scale) + residual;
  }
  const float waveform = parameters.morph;
  const float detune = parameters.timbre * kScaleVoicesMaxDetuneCents;
  const float fold = parameters.timbre;
  plaits_blob_import_scale_voices_render(
      scales_, &voices_, notes, kBlobScaleStackNumVoices, waveform, detune,
      fold, out, aux, size);
}

}  // namespace plaits

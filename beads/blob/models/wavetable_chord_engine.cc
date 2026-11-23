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
// WavetableChord: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/wavetable_chord_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"


namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const int kWavetableChordNumChords = 16;
const int kWavetableChordMaxExtensions = 3;

const int8_t kChordExtensions[kWavetableChordNumChords][
    kWavetableChordMaxExtensions + 1] = {
  { 1, 7, 0, 0 }, { 2, 5, 7, 0 }, { 1, 6, 0, 0 }, { 2, 5, 6, 0 },
  { 2, 6, 8, 0 }, { 3, 6, 8, 10 }, { 3, 5, 7, 10 }, { 1, 8, 0, 0 },
  { 2, 6, 10, 0 }, { 2, 6, 12, 0 }, { 1, 7, 0, 0 }, { 1, 6, 0, 0 },
  { 2, 6, 8, 0 }, { 2, 6, 8, 0 }, { 3, 8, 10, 6 }, { 0, 0, 0, 0 },
};

inline int ThirdDegree(int chord) {
  return chord < 11 ? 2 : (chord < 15 ? 3 : 1);
}

const int kFifthDegree = 4;

}  // namespace

void WavetableChord::Init(
    const beads::PlaitsBlobLibrary* scales,
    const beads::PlaitsBlobLibrary* wavetable,
    BufferAllocator* allocator) {
  scales_ = scales;
  wavetable_ = wavetable;
  num_scales_ = *static_cast<const int*>(
      scales_->exports[beads::SCALE_VOICES_NUM_SCALES]);
  (void) allocator;
  plaits_blob_import_scale_voices_init(scales_, &voices_);
}

void WavetableChord::Reset() {
  plaits_blob_import_scale_voices_reset(scales_, &voices_);
}

void WavetableChord::Render(
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
  int chord = static_cast<int>(
      parameters.harmonics * kWavetableChordNumChords);
  CONSTRAIN(chord, 0, kWavetableChordNumChords - 1);

  float residual = 0.0f;
  const int root_degree = plaits_blob_import_scale_voices_quantize(
      scales_, &voices_, parameters.note, scale, &residual);
  float notes[kScaleVoicesMaxVoices];
  int num_voices = 0;
  notes[num_voices++] = parameters.note;
  notes[num_voices++] = plaits_blob_import_scale_voices_degree_to_note(
      scales_, root_degree + ThirdDegree(chord), scale) + residual;
  notes[num_voices++] = plaits_blob_import_scale_voices_degree_to_note(
      scales_, root_degree + kFifthDegree, scale) + residual;
  const int num_extensions = kChordExtensions[chord][0];
  for (int i = 0; i < num_extensions; ++i) {
    int degree = kChordExtensions[chord][1 + i];
    CONSTRAIN(degree, 0, kScaleVoicesMaxDegreeOffset);
    notes[num_voices++] = plaits_blob_import_scale_voices_degree_to_note(
        scales_, root_degree + degree, scale) + residual;
  }

  const float detune = parameters.timbre * kScaleVoicesMaxDetuneCents;
  plaits_blob_import_wavetable_scale_voices_render(
      wavetable_, &voices_, notes, num_voices, parameters.morph, detune,
      out, aux, size);
}

}  // namespace plaits

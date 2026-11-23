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
// DiatonicChord: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/diatonic_chord_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const int8_t kChordExtensions[kBlobDiatonicChordNumChords][
    kBlobDiatonicChordMaxExtensions + 1] = {
  { 1, 7, 0, 0 },    // octave
  { 2, 5, 7, 0 },    // octave add6
  { 1, 6, 0, 0 },    // seventh
  { 2, 5, 6, 0 },    // seventh add6
  { 2, 6, 8, 0 },    // ninth
  { 3, 6, 8, 10 },   // eleventh
  { 3, 5, 7, 10 },   // eleventh add6
  { 1, 8, 0, 0 },    // add9
  { 2, 6, 10, 0 },   // seventh add11
  { 2, 6, 12, 0 },   // seventh add13
  { 1, 7, 0, 0 },    // octave sus4
  { 1, 6, 0, 0 },    // seventh sus4
  { 2, 6, 8, 0 },    // ninth sus4
  { 2, 6, 8, 0 },    // ninth sus4, wider
  { 3, 8, 10, 6 },   // eleventh sus4
  { 0, 0, 0, 0 },    // sus2 triad
};

inline int ThirdDegree(int chord) {
  if (chord < 11) {
    return 2;
  } else if (chord < 15) {
    return 3;
  } else {
    return 1;
  }
}

const int kFifthDegree = 4;

}  // namespace

void DiatonicChord::Init(
    const beads::PlaitsBlobLibrary* scales,
    BufferAllocator* allocator) {
  scales_ = scales;
  num_scales_ = *static_cast<const int*>(
      scales_->exports[beads::SCALE_VOICES_NUM_SCALES]);
  (void) allocator;
  plaits_blob_import_scale_voices_init(scales_, &voices_);
}

void DiatonicChord::Reset() {
  plaits_blob_import_scale_voices_reset(scales_, &voices_);
}

void DiatonicChord::Render(
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
      parameters.harmonics * kBlobDiatonicChordNumChords);
  CONSTRAIN(chord, 0, kBlobDiatonicChordNumChords - 1);

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

  const float waveform = parameters.morph;
  const float detune = parameters.timbre * kScaleVoicesMaxDetuneCents;
  const float fold = parameters.timbre;
  plaits_blob_import_scale_voices_render(
      scales_, &voices_, notes, num_voices, waveform, detune, fold,
      out, aux, size);
}

}  // namespace plaits

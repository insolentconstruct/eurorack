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
// Host bindings for plain-class scale model sanitizer tests.

#ifndef BEADS_TEST_SCALE_VOICES_STUBS_H_
#define BEADS_TEST_SCALE_VOICES_STUBS_H_

#include "beads/blob/libraries/scale_voices_library.h"
#include "beads/blob/libraries/wavetable_scale_voices_library.h"
#include "plaits/resources.h"

void plaits_blob_import_scale_voices_init(
    const beads::PlaitsBlobLibrary* library, plaits::ScaleVoiceBank* voices) {
  voices->Init();
}

void plaits_blob_import_scale_voices_reset(
    const beads::PlaitsBlobLibrary* library, plaits::ScaleVoiceBank* voices) {
  voices->Reset();
}

int plaits_blob_import_scale_voices_quantize(
    const beads::PlaitsBlobLibrary* library, plaits::ScaleVoiceBank* voices,
    float note, int scale, float* residual) {
  return voices->Quantize(note, scale, residual);
}

float plaits_blob_import_scale_voices_degree_to_note(
    const beads::PlaitsBlobLibrary* library, int degree, int scale) {
  return plaits::ScaleDegreeToNote(degree, scale);
}

void plaits_blob_import_scale_voices_render(
    const beads::PlaitsBlobLibrary* library, plaits::ScaleVoiceBank* voices,
    const float* notes, int num_voices, float waveform, float detune_cents,
    float fold, float* out, float* aux, size_t size) {
  voices->Render(notes, num_voices, waveform, detune_cents, fold, out, aux, size);
}

void plaits_blob_import_wavetable_scale_voices_render(
    const beads::PlaitsBlobLibrary* library, plaits::ScaleVoiceBank* voices,
    const float* notes, int num_voices, float scan, float detune_cents,
    float* out, float* aux, size_t size) {
  voices->RenderWavetable(plaits::wav_integrated_waves, notes, num_voices,
      scan, detune_cents, out, aux, size);
}

#endif  // BEADS_TEST_SCALE_VOICES_STUBS_H_

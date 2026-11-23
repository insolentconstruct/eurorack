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
// Palette's scale voices as a library: state belongs to each dependent.

#ifndef BEADS_BLOB_LIBRARIES_SCALE_VOICES_LIBRARY_H_
#define BEADS_BLOB_LIBRARIES_SCALE_VOICES_LIBRARY_H_

#include "beads/blob/plaits_blob.h"
#include "beads/blob/libraries/scale_voices.h"

namespace beads {

const uint32_t kScaleVoicesLibrary = 5;

enum ScaleVoicesLibraryExport {
  SCALE_VOICES_INIT,
  SCALE_VOICES_RESET,
  SCALE_VOICES_QUANTIZE,
  SCALE_VOICES_DEGREE_TO_NOTE,
  SCALE_VOICES_RENDER,
  SCALE_VOICES_NUM_SCALES,
  SCALE_VOICES_LIBRARY_EXPORT_LAST
};

typedef void ScaleVoicesInit(
    const PlaitsBlobLibrary* library,
    plaits::ScaleVoiceBank* voices);
typedef void ScaleVoicesReset(
    const PlaitsBlobLibrary* library,
    plaits::ScaleVoiceBank* voices);
typedef int ScaleVoicesQuantize(
    const PlaitsBlobLibrary* library,
    plaits::ScaleVoiceBank* voices,
    float note,
    int scale,
    float* residual);
typedef float ScaleVoicesDegreeToNote(
    const PlaitsBlobLibrary* library,
    int degree,
    int scale);
typedef void ScaleVoicesRender(
    const PlaitsBlobLibrary* library,
    plaits::ScaleVoiceBank* voices,
    const float* notes,
    int num_voices,
    float waveform,
    float detune_cents,
    float fold,
    float* out,
    float* aux,
    size_t size);

}  // namespace beads

extern "C" {

beads::ScaleVoicesInit scale_voices_init;
beads::ScaleVoicesReset scale_voices_reset;
beads::ScaleVoicesQuantize scale_voices_quantize;
beads::ScaleVoicesDegreeToNote scale_voices_degree_to_note;
beads::ScaleVoicesRender scale_voices_render;
beads::ScaleVoicesInit plaits_blob_import_scale_voices_init;
beads::ScaleVoicesReset plaits_blob_import_scale_voices_reset;
beads::ScaleVoicesQuantize plaits_blob_import_scale_voices_quantize;
beads::ScaleVoicesDegreeToNote plaits_blob_import_scale_voices_degree_to_note;
beads::ScaleVoicesRender plaits_blob_import_scale_voices_render;

}  // extern "C"

#endif  // BEADS_BLOB_LIBRARIES_SCALE_VOICES_LIBRARY_H_

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
// A dependent's calls to the scale-voice library.

#include "beads/blob/libraries/scale_voices_library.h"

using namespace beads;
using namespace plaits;

template<typename T>
inline T* Export(const PlaitsBlobLibrary* library, ScaleVoicesLibraryExport e) {
  return reinterpret_cast<T*>(library->exports[e]);
}

void plaits_blob_import_scale_voices_init(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices) {
  Export<ScaleVoicesInit>(library, SCALE_VOICES_INIT)(library, voices);
}

void plaits_blob_import_scale_voices_reset(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices) {
  Export<ScaleVoicesReset>(library, SCALE_VOICES_RESET)(library, voices);
}

int plaits_blob_import_scale_voices_quantize(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices,
    float note,
    int scale,
    float* residual) {
  return Export<ScaleVoicesQuantize>(library, SCALE_VOICES_QUANTIZE)(
      library, voices, note, scale, residual);
}

float plaits_blob_import_scale_voices_degree_to_note(
    const PlaitsBlobLibrary* library,
    int degree,
    int scale) {
  return Export<ScaleVoicesDegreeToNote>(library, SCALE_VOICES_DEGREE_TO_NOTE)(
      library, degree, scale);
}

void plaits_blob_import_scale_voices_render(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices,
    const float* notes,
    int num_voices,
    float waveform,
    float detune_cents,
    float fold,
    float* out,
    float* aux,
    size_t size) {
  Export<ScaleVoicesRender>(library, SCALE_VOICES_RENDER)(
      library, voices, notes, num_voices, waveform, detune_cents, fold,
      out, aux, size);
}

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
// Palette's scale-voice code, with state kept by each dependent.

#include "beads/blob/libraries/scale_voices_library.h"

using namespace beads;
using namespace plaits;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  kPlaitsBlobAbi2,
  kPlaitsBlobServicesVersion2,
  kPlaitsBlobKindLibrary,
  kScaleVoicesLibrary,
  1,
  0,  // The packer fills the size, export table and GOT fields.
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  { 0, 0, 0 },
  { 0, 0, 0, 0 }
};

void scale_voices_init(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices) {
  voices->Init();
}

void scale_voices_reset(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices) {
  voices->Reset();
}

int scale_voices_quantize(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices,
    float note,
    int scale,
    float* residual) {
  return voices->Quantize(note, scale, residual);
}

float scale_voices_degree_to_note(
    const PlaitsBlobLibrary* library,
    int degree,
    int scale) {
  return ScaleDegreeToNote(degree, scale);
}

void scale_voices_render(
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
  voices->Render(notes, num_voices, waveform, detune_cents, fold, out, aux, size);
}

// In ScaleVoicesLibraryExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  reinterpret_cast<const void*>(&scale_voices_init),
  reinterpret_cast<const void*>(&scale_voices_reset),
  reinterpret_cast<const void*>(&scale_voices_quantize),
  reinterpret_cast<const void*>(&scale_voices_degree_to_note),
  reinterpret_cast<const void*>(&scale_voices_render),
  &kScaleVoicesNumScales
};

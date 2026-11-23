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
// Palette's wavetable scale-voice code, bound to the wavetable library.

#include "beads/blob/libraries/wavetable_scale_voices_library.h"
#include "beads/blob/libraries/plaits_wavetables.h"

using namespace beads;
using namespace plaits;

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  kPlaitsBlobAbi2,
  kPlaitsBlobServicesVersion2,
  kPlaitsBlobKindLibrary,
  kWavetableScaleVoicesLibrary,
  1,
  0,  // The packer fills the size, init, export table and GOT fields.
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  1,  // The Plaits wavetable library.
  { 0, 0, 0 },
  { 0, 0, 0, 0 }
};

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    uint8_t* scratch,
    size_t scratch_size,
    const PlaitsBlobServices* services,
    const PlaitsBlobLibrary* const* libraries) {
  return const_cast<void*>(libraries[0]->exports[PLAITS_WAVETABLES_WAVES]);
}

void wavetable_scale_voices_render(
    const PlaitsBlobLibrary* library,
    ScaleVoiceBank* voices,
    const float* notes,
    int num_voices,
    float scan,
    float detune_cents,
    float* out,
    float* aux,
    size_t size) {
  const int16_t* waves = static_cast<const int16_t*>(library->context);
  voices->RenderWavetable(
      waves, notes, num_voices, scan, detune_cents, out, aux, size);
}

// In WavetableScaleVoicesLibraryExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  reinterpret_cast<const void*>(&wavetable_scale_voices_render)
};

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
//
// -----------------------------------------------------------------------------
// Exercise the exact libm copies through the blob entry points.

#include <math.h>
#include "beads/test/libm_blob_emu/diagnostics.h"
#include "beads/blob/plaits_blob.h"

extern "C" const beads::PlaitsBlobHeader2 plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  beads::kPlaitsBlobMagic, sizeof(beads::PlaitsBlobHeader2),
  beads::kPlaitsBlobAbi2, beads::kPlaitsBlobServicesVersion2,
  beads::kPlaitsBlobKindModel, 254, 1, 0, sizeof(Diagnostics), 0,
  0, 0, 0, 0, 0, 0, 0, 0, 1.0f, 1.0f, { 0, 0, 0, 0 }
};

extern "C" void* plaits_blob_init(void* state,
    const beads::PlaitsBlobServices* services, uint8_t* arena,
    size_t arena_size, const beads::PlaitsBlobLibrary* const* libraries) {
  return state;
}

extern "C" void plaits_blob_reset(void* object,
    const beads::PlaitsBlobServices* services, uint32_t argument) { }

extern "C" void plaits_blob_render(void* object,
    const beads::PlaitsBlobServices* services,
    const plaits::EngineParameters* parameters, float* out, float* aux,
    size_t size, bool* already_enveloped) {
  Diagnostics* diagnostics = static_cast<Diagnostics*>(object);
  for (size_t i = 0; i < size; ++i) {
    const float x = out[i];
    ClearExceptions(diagnostics->control);
    out[i] = sinf(x);
    diagnostics->flags[0][i] = Exceptions();
    ClearExceptions(diagnostics->control);
    aux[i] = tanhf(x);
    diagnostics->flags[1][i] = Exceptions();
    ClearExceptions(diagnostics->control);
    diagnostics->expm1[i] = expm1f(x);
    diagnostics->flags[2][i] = Exceptions();
  }
}

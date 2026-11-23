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
// The reference calls the pinned toolchain's libm, with no blob copies.

#include <math.h>
#include <stddef.h>
#include "beads/test/libm_blob_emu/diagnostics.h"

float input[4096];
float out[4096];
float aux[4096];
Diagnostics diagnostics;

void reference(size_t size) {
  for (size_t i = 0; i < size; ++i) {
    ClearExceptions(diagnostics.control);
    out[i] = sinf(input[i]);
    diagnostics.flags[0][i] = Exceptions();
    ClearExceptions(diagnostics.control);
    aux[i] = tanhf(input[i]);
    diagnostics.flags[1][i] = Exceptions();
    ClearExceptions(diagnostics.control);
    diagnostics.expm1[i] = expm1f(input[i]);
    diagnostics.flags[2][i] = Exceptions();
  }
}

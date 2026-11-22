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
//
// Gate controls for libraries: a test library's interface and a model that
// calls it. IMPORT_BODY and NUM_DEPENDENCIES may be predefined.

#ifndef BEADS_TEST_BLOB_GATE_COMMON_LIBRARY_H_
#define BEADS_TEST_BLOB_GATE_COMMON_LIBRARY_H_

#define NUM_DEPENDENCIES_DEFAULT 1
#ifndef NUM_DEPENDENCIES
#define NUM_DEPENDENCIES NUM_DEPENDENCIES_DEFAULT
#endif  // NUM_DEPENDENCIES

#include "beads/test/blob_gate/common2.h"

// The test library (pos_library.cc): ID 200, one export.
typedef void TestScale(
    const PlaitsBlobLibrary* library,
    float* out,
    size_t size);

#ifndef IMPORT_BODY
#define IMPORT_BODY \
    reinterpret_cast<TestScale*>(library->exports[0])(library, out, size);
#endif  // IMPORT_BODY

// Out of line, as a dependent's own translation unit would keep it.
extern "C" __attribute__((noinline)) void plaits_blob_import_test_scale(
    const PlaitsBlobLibrary* library,
    float* out,
    size_t size) {
  IMPORT_BODY
}

#endif  // BEADS_TEST_BLOB_GATE_COMMON_LIBRARY_H_

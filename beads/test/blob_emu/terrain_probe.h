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
// Forced into the harness's compiled-in Wave terrain: counts the lookups whose
// wave or sample position leaves the bank, which the blob clamps.

#ifndef BEADS_TEST_BLOB_EMU_TERRAIN_PROBE_H_
#define BEADS_TEST_BLOB_EMU_TERRAIN_PROBE_H_

#include "stmlib/dsp/dsp.h"

extern "C" uint32_t harness_terrain_overshoot;

inline void HarnessTerrainCheck(const char* name, float value) {
  const bool sample = name[0] == 's' && name[1] == 'a';
  const bool grid = (name[0] == 'x' || name[0] == 'y') && !name[1];
  const bool wt = name[0] == 'w' && name[1] == 't';
  if ((sample && (value < 0.0f || value > 128.0f)) ||
      ((wt || grid) && (value < 0.0f || value > 62.9999f))) {
    ++harness_terrain_overshoot;
  }
}

#undef MAKE_INTEGRAL_FRACTIONAL
#define MAKE_INTEGRAL_FRACTIONAL(x) \
  HarnessTerrainCheck(#x, x); \
  int32_t x ## _integral = static_cast<int32_t>(x); \
  float x ## _fractional = x - static_cast<float>(x ## _integral);

#endif  // BEADS_TEST_BLOB_EMU_TERRAIN_PROBE_H_

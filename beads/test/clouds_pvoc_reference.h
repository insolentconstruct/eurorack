// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Clouds' phase vocoder as Clouds ships it (ShyFFT), for comparison with the
// Beads port.

#ifndef BEADS_TEST_CLOUDS_PVOC_REFERENCE_H_
#define BEADS_TEST_CLOUDS_PVOC_REFERENCE_H_

#include "stmlib/stmlib.h"

namespace clouds_reference {

struct Settings {
  float position;
  float pitch;
  bool freeze;
  bool gate;
  float quantization;
  float refresh_rate;
  float phase_randomization;
  float warp;
};

// Zeroes two 256 KB buffers, as the Beads engine does, then starts Clouds'
// phase vocoder (4096-point frames) on them.
void Init(int num_channels);

// Interleaved stereo frames.
void Process(const Settings& settings, const float* in, float* out,
             size_t size);

void Buffer();

}  // namespace clouds_reference

#endif  // BEADS_TEST_CLOUDS_PVOC_REFERENCE_H_

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
// Include first so service exports use the GOT and blob symbols stay
// PC-relative (see ABI.md).

#ifndef BEADS_BLOB_BLOB_VISIBILITY_H_
#define BEADS_BLOB_BLOB_VISIBILITY_H_

#include "stmlib/stmlib.h"

#include "stmlib/utils/random.h"

namespace stmlib {

extern const float lut_pitch_ratio_high[257]
    __attribute__((visibility("default")));
extern const float lut_pitch_ratio_low[257]
    __attribute__((visibility("default")));

}  // namespace stmlib

namespace plaits {

extern const float lut_sine[] __attribute__((visibility("default")));
extern const float lut_fm_frequency_quantizer[]
    __attribute__((visibility("default")));
extern const float lut_fold[] __attribute__((visibility("default")));
extern const float lut_4x_downsampler_fir[]
    __attribute__((visibility("default")));
extern const int16_t lut_ws_inverse_tan[]
    __attribute__((visibility("default")));
extern const int16_t lut_ws_inverse_sin[]
    __attribute__((visibility("default")));
extern const int16_t lut_ws_linear[] __attribute__((visibility("default")));
extern const int16_t lut_ws_bump[] __attribute__((visibility("default")));

}  // namespace plaits

#pragma GCC visibility push(hidden)

#endif  // BEADS_BLOB_BLOB_VISIBILITY_H_

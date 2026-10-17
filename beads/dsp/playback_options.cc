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
// Grain playback options.

#include "beads/dsp/playback_options.h"

namespace beads {

// One pole at block rate; rounding the fixed-point step up reaches either
// endpoint exactly without a final jump or a floating-point convergence stall.
DelayFilterQ SmoothDelayFilterQ(
    DelayFilterMode* mode, float offset, float scale) {
  if (mode->wide_detent) {
    mode->position += (65535 - mode->position + 199) / 200;
  } else {
    mode->position -= (mode->position + 199) / 200;
  }
  float blend = float(mode->position) * (1.0f / 65535.0f);
  if (mode->wide_detent) {
    blend = 1.0f - blend;
  }
  const float previous_offset = mode->wide_detent ? 1.0f : 0.70710678f;
  const float previous_scale = mode->wide_detent ? 16.0f : 0.0f;
  DelayFilterQ q;
  q.offset = offset + (previous_offset - offset) * blend;
  q.scale = scale + (previous_scale - scale) * blend;
  return q;
}

}  // namespace beads

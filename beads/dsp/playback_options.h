// Copyright 2026 OpenAI.
//
// Author: OpenAI
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

#ifndef BEADS_DSP_PLAYBACK_OPTIONS_H_
#define BEADS_DSP_PLAYBACK_OPTIONS_H_

namespace beads {

// Saved byte: zero is stock, 1..255 spans zero to full displacement.
typedef unsigned char StereoWidth;
const StereoWidth STEREO_WIDTH_STOCK = 0;

inline float ModulatedStereoWidth(StereoWidth width, float cv) {
  if (!width) return 0.0f;
  float amount = float(width - 1) / 254.0f + cv;
  if (amount < 0.0f) amount = 0.0f;
  if (amount > 1.0f) amount = 1.0f;
  return 1.0f + 254.0f * amount;
}

enum DelayMode {
  DELAY_MODE_AUTO,
  DELAY_MODE_NEVER,
  DELAY_MODE_WIDE_DETENT
};

inline float DelayFilterDetent(float bandwidth) {
  if (bandwidth < 0.4f) {
    return bandwidth * 1.25f;
  } else if (bandwidth > 0.6f) {
    return 0.5f + (bandwidth - 0.6f) * 1.25f;
  }
  return 0.5f;
}

}  // namespace beads

#endif  // BEADS_DSP_PLAYBACK_OPTIONS_H_

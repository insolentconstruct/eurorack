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

#ifndef BEADS_DSP_PLAYBACK_OPTIONS_H_
#define BEADS_DSP_PLAYBACK_OPTIONS_H_

#include "stmlib/stmlib.h"

namespace beads {

const float kDelayDetentLow = 0.4f;
const float kDelayDetentHigh = 0.6f;
const float kStockDelayQOffset = 1.0f;
const float kStockDelayQScale = 16.0f;
const float kWideDetentQOffset = 0.70710678f;
const int kDelayQRampSteps = 200;
const int kDelayQPositionMax = 65535;

// Saved byte: zero is stock, 1..255 spans zero to full displacement.
typedef uint8_t StereoWidth;
const int kStereoWidthMax = 255;

inline float ModulatedStereoWidth(StereoWidth width, float cv) {
  if (!width) {
    return 0.0f;
  }
  float amount = float(width - 1) / float(kStereoWidthMax - 1) + cv;
  CONSTRAIN(amount, 0.0f, 1.0f);
  return amount;
}

// Grain diffusion of the Clouds-style reverb until [A]+REVERB sets an amount.
const float kCloudsDiffusion = 0.7f;

// Delay mode, options page 2, FEEDBACK.
enum DelayMode {
  DELAY_MODE_AUTO,
  DELAY_MODE_NEVER,
  DELAY_MODE_WIDE_DETENT
};

struct DelayFilterMode {
  bool wide_detent;
  uint8_t reserved;
  uint16_t position;
};

struct DelayFilterModeLayoutAssertions {
  STATIC_ASSERT(sizeof(DelayFilterMode) == 4, delay_filter_mode_size);
};

struct DelayFilterQ {
  float offset;
  float scale;
};

DelayFilterQ SmoothDelayFilterQ(
    DelayFilterMode* mode, float offset, float scale);

inline float DelayFilterDetent(float bandwidth) {
  if (bandwidth < kDelayDetentLow) {
    return bandwidth * 1.25f;
  } else if (bandwidth > kDelayDetentHigh) {
    return 0.5f + (bandwidth - kDelayDetentHigh) * 1.25f;
  }
  return 0.5f;
}

}  // namespace beads

#endif  // BEADS_DSP_PLAYBACK_OPTIONS_H_

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
// Fixed-root grain pitch quantization.

#include "beads/dsp/pitch_quantizer.h"

namespace beads {

// Marbles preset notes with weight >= 64, in semitones.
const float kPitchScaleNotes[] = {
  0.0000f, 1.0000f, 2.0000f, 3.0000f, 4.0000f, 5.0000f,
  6.0000f, 7.0000f, 8.0000f, 9.0000f, 10.0000f, 11.0000f,
  0.0000f, 7.0000f,
  0.0000f, 2.0004f, 3.9996f, 5.0004f, 6.9996f, 9.0000f, 11.0004f,
  0.0000f, 2.0004f, 3.0000f, 5.0004f, 6.9996f, 8.0004f, 9.9996f,
  0.0000f, 2.0004f, 5.0004f, 6.9996f, 9.0000f,
  0.0000f, 1.5300f, 7.0596f, 8.4804f,
  0.0000f, 0.9024f, 3.8628f, 4.9800f, 7.0200f, 7.9212f, 10.8828f,
  0.0000f, 2.0388f, 3.1560f, 4.9800f, 7.0200f, 9.0588f, 10.1748f,
};

const PitchScaleNotes kPitchScales[] = {
  { 0, 12 },
  { 12, 2 },
  { 14, 7 },
  { 21, 7 },
  { 28, 5 },
  { 33, 4 },
  { 37, 7 },
  { 44, 7 },
};

float QuantizePitch(float pitch, PitchScale scale) {
  if (scale == PITCH_SCALE_OFF) {
    return pitch;
  }
  if (scale == PITCH_SCALE_CHROMATIC) {
    return ceilf(pitch - 0.5f);
  }
  const float octave = floorf(pitch / 12.0f) * 12.0f;
  const PitchScaleNotes& notes = kPitchScales[scale - 1];
  const float* degrees = &kPitchScaleNotes[notes.offset];
  float lower = octave;
  for (int i = 1; i <= notes.size; ++i) {
    const float upper = octave + (i == notes.size ? 12.0f : degrees[i]);
    if (pitch <= 0.5f * (lower + upper)) {
      return lower;
    }
    lower = upper;
  }
  return lower;
}


}  // namespace beads

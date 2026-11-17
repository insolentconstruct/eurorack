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
// A second, small blob engine for multi-blob tests: a sine at the note, with
// TIMBRE as its level, and its quadrature on AUX.

#ifndef BEADS_TEST_BLOB_GATE_TEST_SINE_H_
#define BEADS_TEST_BLOB_GATE_TEST_SINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace beads {

class TestSine {
 public:
  TestSine() { }
  ~TestSine() { }

  void Init() {
    phase_ = 0.0f;
  }

  void Render(
      const plaits::EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size) {
    if (parameters.trigger & plaits::TRIGGER_RISING_EDGE) {
      phase_ = 0.0f;
    }
    const float frequency = plaits::NoteToFrequency(parameters.note);
    for (size_t i = 0; i < size; ++i) {
      phase_ += frequency;
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      }
      out[i] = parameters.timbre * plaits::SineNoWrap(phase_);
      aux[i] = plaits::Sine(phase_ + 0.25f);
    }
  }

 private:
  float phase_;

  DISALLOW_COPY_AND_ASSIGN(TestSine);
};

}  // namespace beads

#endif  // BEADS_TEST_BLOB_GATE_TEST_SINE_H_

// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// AP diffusion network.

#ifndef BEADS_DSP_FX_DIFFUSER_H_
#define BEADS_DSP_FX_DIFFUSER_H_

#include <algorithm>

#include "stmlib/stmlib.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/fx/fx_engine.h"

namespace beads {

const size_t kDiffuserBufferSize = 2048;

class Diffuser {
 public:
  Diffuser() { }
  ~Diffuser() { }

  void Init(uint16_t* buffer) {
    buffer_ = buffer;
    std::fill(&buffer_[0], &buffer_[kDiffuserBufferSize], 0);
    write_ptr_ = 0;
  }

  void Process(FloatFrame* in_out, size_t size);

  void set_amount(float amount) {
    amount_ = amount;
  }

 private:
  // The engine only lays out the delay lines; the buffer is accessed here.
  typedef FxEngine<kDiffuserBufferSize, FORMAT_16_BIT> E;
  typedef DataType<FORMAT_16_BIT> Format;
  enum {
    MASK = kDiffuserBufferSize - 1
  };

  // Reads the oldest sample of a delay line.
  template<typename D>
  inline float Tail(D& d, int32_t write_ptr) const {
    const int32_t index = write_ptr + D::base + D::length - 1;
    return Format::Decompress(buffer_[index & MASK]);
  }

  // Writes the sum to the head of a delay line, returns the all-pass output.
  template<typename D>
  inline float AllPass(
      D& d, int32_t write_ptr, float in, float tail, float coefficient) {
    const float sum = in + tail * coefficient;
    buffer_[(write_ptr + D::base) & MASK] = Format::Compress(sum);
    return tail - sum * coefficient;
  }

  uint16_t* buffer_;
  int32_t write_ptr_;
  float amount_;
  DISALLOW_COPY_AND_ASSIGN(Diffuser);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_DIFFUSER_H_

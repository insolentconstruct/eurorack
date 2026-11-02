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
const float kDiffuserModulationDepth = 16.0f;  // Samples
const float kAllPassCoefficient = 0.625f;

// FxEngine lays out power-of-two, 16-bit delay lines; access stays here.
// Read every tail before writing to avoid alias-dependent load ordering.
template<size_t size>
class AllPassMemory {
 public:
  AllPassMemory() { }
  ~AllPassMemory() { }

  void Init(uint16_t* buffer) {
    buffer_ = buffer;
    std::fill(&buffer_[0], &buffer_[size], 0);
  }

  static inline int32_t Previous(int32_t write_ptr) {
    return (write_ptr - 1) & MASK;
  }

  // Reads the oldest sample of a delay line.
  template<typename D>
  inline float Tail(D& d, int32_t write_ptr) const {
    const int32_t index = write_ptr + D::base + D::length - 1;
    return Format::Decompress(buffer_[index & MASK]);
  }

  // Reads a delay line at a fractional offset from its head.
  template<typename D>
  inline float Tail(D& d, int32_t write_ptr, float offset) const {
    MAKE_INTEGRAL_FRACTIONAL(offset);
    const int32_t index = write_ptr + offset_integral + D::base;
    const float a = Format::Decompress(buffer_[index & MASK]);
    const float b = Format::Decompress(buffer_[(index + 1) & MASK]);
    return a + (b - a) * offset_fractional;
  }

  // Writes the sum to the head of a delay line, returns the all-pass output.
  template<typename D>
  inline float AllPass(
      D& d, int32_t write_ptr, float in, float tail, float coefficient) {
    const float sum = in + tail * coefficient;
    buffer_[(write_ptr + D::base) & MASK] = Format::Compress(sum);
    return tail - sum * coefficient;
  }

 private:
  typedef DataType<FORMAT_16_BIT> Format;
  enum {
    MASK = size - 1
  };

  uint16_t* buffer_;
  DISALLOW_COPY_AND_ASSIGN(AllPassMemory);
};

class Diffuser {
 public:
  Diffuser() { }
  ~Diffuser() { }

  void Init(uint16_t* buffer) {
    memory_.Init(buffer);
    write_ptr_ = 0;
  }

  void Process(FloatFrame* in_out, size_t size);

  void set_amount(float amount) {
    amount_ = amount;
  }

  // Modulation 0..1 moves each channel's longest all-pass tap by up to
  // kDiffuserModulationDepth samples, using interpolation.
  template<bool modulated>
  void Run(FloatFrame* in_out, size_t size, float modulation);

 private:
  AllPassMemory<kDiffuserBufferSize> memory_;
  int32_t write_ptr_;
  float amount_;

  DISALLOW_COPY_AND_ASSIGN(Diffuser);
};

template<bool modulated>
inline void Diffuser::Run(FloatFrame* in_out, size_t size, float modulation) {
  typedef FxEngine<kDiffuserBufferSize, FORMAT_16_BIT> E;
  typedef E::Reserve<126,
    E::Reserve<180,
    E::Reserve<269,
    E::Reserve<444,
    E::Reserve<151,
    E::Reserve<205,
    E::Reserve<245,
    E::Reserve<405> > > > > > > > Memory;
  E::DelayLine<Memory, 0> apl1;
  E::DelayLine<Memory, 1> apl2;
  E::DelayLine<Memory, 2> apl3;
  E::DelayLine<Memory, 3> apl4;
  E::DelayLine<Memory, 4> apr1;
  E::DelayLine<Memory, 5> apr2;
  E::DelayLine<Memory, 6> apr3;
  E::DelayLine<Memory, 7> apr4;
  // The reads of a sample do not depend on its writes, so they are all done
  // first: the processor then overlaps their latency with the filtering.
  const float amount = amount_;
  const float depth = kDiffuserModulationDepth * modulation;
  const float offset_l = float(apl4.length - 2) - depth;
  const float offset_r = float(apr4.length - 2) - depth;
  int32_t write_ptr = write_ptr_;
  while (size--) {
    write_ptr = memory_.Previous(write_ptr);
    const float tl1 = memory_.Tail(apl1, write_ptr);
    const float tl2 = memory_.Tail(apl2, write_ptr);
    const float tl3 = memory_.Tail(apl3, write_ptr);
    const float tl4 = modulated
        ? memory_.Tail(apl4, write_ptr, offset_l)
        : memory_.Tail(apl4, write_ptr);
    const float tr1 = memory_.Tail(apr1, write_ptr);
    const float tr2 = memory_.Tail(apr2, write_ptr);
    const float tr3 = memory_.Tail(apr3, write_ptr);
    const float tr4 = modulated
        ? memory_.Tail(apr4, write_ptr, offset_r)
        : memory_.Tail(apr4, write_ptr);

    float wet = in_out->l;
    wet = memory_.AllPass(apl1, write_ptr, wet, tl1, kAllPassCoefficient);
    wet = memory_.AllPass(apl2, write_ptr, wet, tl2, kAllPassCoefficient);
    wet = memory_.AllPass(apl3, write_ptr, wet, tl3, kAllPassCoefficient);
    wet = memory_.AllPass(apl4, write_ptr, wet, tl4, kAllPassCoefficient);
    in_out->l += amount * (wet - in_out->l);

    wet = in_out->r;
    wet = memory_.AllPass(apr1, write_ptr, wet, tr1, kAllPassCoefficient);
    wet = memory_.AllPass(apr2, write_ptr, wet, tr2, kAllPassCoefficient);
    wet = memory_.AllPass(apr3, write_ptr, wet, tr3, kAllPassCoefficient);
    wet = memory_.AllPass(apr4, write_ptr, wet, tr4, kAllPassCoefficient);
    in_out->r += amount * (wet - in_out->r);

    ++in_out;
  }
  write_ptr_ = write_ptr;
}

}  // namespace beads

#endif  // BEADS_DSP_FX_DIFFUSER_H_

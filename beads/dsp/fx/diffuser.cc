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

#include "beads/dsp/fx/diffuser.h"

namespace beads {

const float kAllPassCoefficient = 0.625f;

void Diffuser::Process(FloatFrame* in_out, size_t size) {
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
  int32_t write_ptr = write_ptr_;
  while (size--) {
    write_ptr = (write_ptr - 1) & MASK;
    const float tl1 = Tail(apl1, write_ptr);
    const float tl2 = Tail(apl2, write_ptr);
    const float tl3 = Tail(apl3, write_ptr);
    const float tl4 = Tail(apl4, write_ptr);
    const float tr1 = Tail(apr1, write_ptr);
    const float tr2 = Tail(apr2, write_ptr);
    const float tr3 = Tail(apr3, write_ptr);
    const float tr4 = Tail(apr4, write_ptr);

    float wet = in_out->l;
    wet = AllPass(apl1, write_ptr, wet, tl1, kAllPassCoefficient);
    wet = AllPass(apl2, write_ptr, wet, tl2, kAllPassCoefficient);
    wet = AllPass(apl3, write_ptr, wet, tl3, kAllPassCoefficient);
    wet = AllPass(apl4, write_ptr, wet, tl4, kAllPassCoefficient);
    in_out->l += amount * (wet - in_out->l);

    wet = in_out->r;
    wet = AllPass(apr1, write_ptr, wet, tr1, kAllPassCoefficient);
    wet = AllPass(apr2, write_ptr, wet, tr2, kAllPassCoefficient);
    wet = AllPass(apr3, write_ptr, wet, tr3, kAllPassCoefficient);
    wet = AllPass(apr4, write_ptr, wet, tr4, kAllPassCoefficient);
    in_out->r += amount * (wet - in_out->r);

    ++in_out;
  }
  write_ptr_ = write_ptr;
}

}  // namespace beads

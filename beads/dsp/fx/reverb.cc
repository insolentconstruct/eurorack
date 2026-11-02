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
// Reverb.

#include "beads/dsp/fx/reverb.h"

#include "beads/dsp/frame.h"

namespace beads {

const float kAllPassCoefficient = 0.625f;

void Reverb::ProcessClouds(FloatFrame* in_out, size_t size) {
  // Dattorro/Griesinger: four input APs, then two 2AP+delay blocks in a loop.
  // Modulate the first diffuser for smear and long delays for shimmer.
  typedef E::Reserve<113,
    E::Reserve<162,
    E::Reserve<241,
    E::Reserve<399,
    E::Reserve<1653,
    E::Reserve<2038,
    E::Reserve<3411,
    E::Reserve<1913,
    E::Reserve<1663,
    E::Reserve<4782> > > > > > > > > > Memory;
  E::DelayLine<Memory, 0> ap1;
  E::DelayLine<Memory, 1> ap2;
  E::DelayLine<Memory, 2> ap3;
  E::DelayLine<Memory, 3> ap4;
  E::DelayLine<Memory, 4> dap1a;
  E::DelayLine<Memory, 5> dap1b;
  E::DelayLine<Memory, 6> del1;
  E::DelayLine<Memory, 7> dap2a;
  E::DelayLine<Memory, 8> dap2b;
  E::DelayLine<Memory, 9> del2;
  E::Context c;

  const float klp = lp_;
  const float krt = reverb_time_;

  const float input_gain = 0.2f * (1.0f - 0.333f * reverb_time_);
  const float output_gain = 0.4f / input_gain;

  const float send_amount_sqrt = mix_ * (2.0f - mix_);
  const float send_amount = send_amount_sqrt * (2.0f - send_amount_sqrt);
  const float wet = output_gain;
  const float dry = (1.0f - mix_) * (1.0f + mix_);

  stmlib::ParameterInterpolator wet_modulation(&wet_, wet, size);
  stmlib::ParameterInterpolator dry_modulation(&dry_, dry, size);
  stmlib::ParameterInterpolator send_modulation(
      &send_,
      send_amount * input_gain,
      size);

  while (size--) {
    const float dry_amount = dry_modulation.Next();
    const float wet_amount = wet_modulation.Next();
    const float send_amount = send_modulation.Next();

    float wet;
    float apout = 0.0f;
    engine_.Start(&c);

    // Smear AP1 inside the loop.
    c.Interpolate(ap1, 10.0f, LFO_1, 60.0f, 1.0f);
    c.Write(ap1, 100, 0.0f);

    c.Read(in_out->l + in_out->r, send_amount);

    // Diffuse through 4 allpasses.
    c.Read(ap1 TAIL, kAllPassCoefficient);
    c.WriteAllPass(ap1, -kAllPassCoefficient);
    c.Read(ap2 TAIL, kAllPassCoefficient);
    c.WriteAllPass(ap2, -kAllPassCoefficient);
    c.Read(ap3 TAIL, kAllPassCoefficient);
    c.WriteAllPass(ap3, -kAllPassCoefficient);
    c.Read(ap4 TAIL, kAllPassCoefficient);
    c.WriteAllPass(ap4, -kAllPassCoefficient);
    c.Write(apout);

    // Main reverb loop.
    c.Load(apout);
    c.Interpolate(del2, 4680.0f, LFO_2, 100.0f, krt);
    c.Lp(lp_1_, klp);
    c.Read(dap1a TAIL, -kAllPassCoefficient);
    c.WriteAllPass(dap1a, kAllPassCoefficient);
    c.Read(dap1b TAIL, kAllPassCoefficient);
    c.WriteAllPass(dap1b, -kAllPassCoefficient);
    c.Write(del1, 2.0f);
    c.Write(wet, 0.0f);

    in_out->l = in_out->l * dry_amount + wet * wet_amount;

    c.Load(apout);
    c.Read(del1 TAIL, krt);
    c.Lp(lp_2_, klp);
    c.Read(dap2a TAIL, kAllPassCoefficient);
    c.WriteAllPass(dap2a, -kAllPassCoefficient);
    c.Read(dap2b TAIL, -kAllPassCoefficient);
    c.WriteAllPass(dap2b, kAllPassCoefficient);
    c.Write(del2, 2.0f);
    c.Write(wet, 0.0f);

    in_out->r = in_out->r * dry_amount + wet * wet_amount;
    ++in_out;
  }
}

}  // namespace beads

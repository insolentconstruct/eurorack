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

void Reverb::set_character(uint8_t character) {
  switch (character) {
    case REVERB_CLOUDS:
      renderer_ = RenderClouds;
      break;
    case REVERB_PLATE:
      renderer_ = RenderPlate;
      break;
    case REVERB_BLOOM:
      renderer_ = RenderBloom;
      break;
    default:
      renderer_ = 0;
      break;
  }
}

/* static */
void Reverb::RenderClouds(Reverb* self, FloatFrame* in_out, size_t size) {
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

  const float klp = self->lp_;
  const float krt = self->reverb_time_;

  const float input_gain = 0.2f * (1.0f - 0.333f * self->reverb_time_);
  const float output_gain = 0.4f / input_gain;

  const float send_amount_sqrt = self->mix_ * (2.0f - self->mix_);
  const float send_amount = send_amount_sqrt * (2.0f - send_amount_sqrt);
  const float wet = output_gain;
  const float dry = (1.0f - self->mix_) * (1.0f + self->mix_);

  stmlib::ParameterInterpolator wet_modulation(&self->wet_, wet, size);
  stmlib::ParameterInterpolator dry_modulation(&self->dry_, dry, size);
  stmlib::ParameterInterpolator send_modulation(
      &self->send_,
      send_amount * input_gain,
      size);

  while (size--) {
    const float dry_amount = dry_modulation.Next();
    const float wet_amount = wet_modulation.Next();
    const float send_amount = send_modulation.Next();

    float wet;
    float apout = 0.0f;
    self->engine_.Start(&c);

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
    c.Lp(self->lp_1_, klp);
    c.Read(dap1a TAIL, -kAllPassCoefficient);
    c.WriteAllPass(dap1a, kAllPassCoefficient);
    c.Read(dap1b TAIL, kAllPassCoefficient);
    c.WriteAllPass(dap1b, -kAllPassCoefficient);
    c.Write(del1, 2.0f);
    c.Write(wet, 0.0f);

    in_out->l = in_out->l * dry_amount + wet * wet_amount;

    c.Load(apout);
    c.Read(del1 TAIL, krt);
    c.Lp(self->lp_2_, klp);
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

/* static */
void Reverb::RenderBloom(Reverb* self, FloatFrame* in_out, size_t size) {
  self->ProcessTank<true>(in_out, size);
}

/* static */
void Reverb::RenderPlate(Reverb* self, FloatFrame* in_out, size_t size) {
  // Figure-eight tank with damped long delays between decay diffusers.
  typedef E::Reserve<109,
    E::Reserve<173,
    E::Reserve<263,
    E::Reserve<397,
    E::Reserve<809,
    E::Reserve<3319,
    E::Reserve<1801,
    E::Reserve<4817,
    E::Reserve<1013,
    E::Reserve<3767,
    E::Reserve<2203,
    E::Reserve<4337> > > > > > > > > > > > Memory;
  E::DelayLine<Memory, 0> ap1;
  E::DelayLine<Memory, 1> ap2;
  E::DelayLine<Memory, 2> ap3;
  E::DelayLine<Memory, 3> ap4;
  E::DelayLine<Memory, 4> mod1;
  E::DelayLine<Memory, 5> delay1;
  E::DelayLine<Memory, 6> decay1;
  E::DelayLine<Memory, 7> return1;
  E::DelayLine<Memory, 8> mod2;
  E::DelayLine<Memory, 9> delay2;
  E::DelayLine<Memory, 10> decay2;
  E::DelayLine<Memory, 11> return2;
  E::Context c;

  const float krt = self->reverb_time_;
  const float damping = 0.3f + 0.7f * self->lp_;
  const float input_gain = 0.2f * (1.0f - 0.333f * krt);
  const float send_sqrt = self->mix_ * (2.0f - self->mix_);
  stmlib::ParameterInterpolator send_modulation(
      &self->send_, send_sqrt * (2.0f - send_sqrt) * input_gain, size);
  stmlib::ParameterInterpolator dry_modulation(
      &self->dry_, (1.0f - self->mix_) * (1.0f + self->mix_), size);
  stmlib::ParameterInterpolator wet_modulation(
      &self->wet_, 0.4f / input_gain, size);

  while (size--) {
    const float send = send_modulation.Next();
    const float dry = dry_modulation.Next();
    const float wet = wet_modulation.Next();
    self->engine_.Start(&c);

    c.Load((in_out->l + in_out->r) * send);
    c.Read(ap1 TAIL, 0.75f);
    c.WriteAllPass(ap1, -0.75f);
    c.Read(ap2 TAIL, 0.75f);
    c.WriteAllPass(ap2, -0.75f);
    c.Read(ap3 TAIL, 0.625f);
    c.WriteAllPass(ap3, -0.625f);
    c.Read(ap4 TAIL, 0.625f);
    c.WriteAllPass(ap4, -0.625f);
    float input;
    c.Write(input);

    c.Read(return2 TAIL, krt);
    c.Interpolate(mod1, 785.0f, LFO_1, 19.0f, -0.7f);
    c.WriteAllPass(mod1, 0.7f);
    c.Write(delay1, 0.0f);
    c.Read(delay1 TAIL, 1.0f);
    c.Lp(self->lp_1_, damping);
    c.Read(decay1 TAIL, 0.5f);
    c.WriteAllPass(decay1, -0.5f);
    c.Write(return1, 0.0f);

    c.Load(input);
    c.Read(return1 TAIL, krt);
    c.Interpolate(mod2, 985.0f, LFO_2, 23.0f, -0.7f);
    c.WriteAllPass(mod2, 0.7f);
    c.Write(delay2, 0.0f);
    c.Read(delay2 TAIL, 1.0f);
    c.Lp(self->lp_2_, damping);
    c.Read(decay2 TAIL, 0.5f);
    c.WriteAllPass(decay2, -0.5f);
    c.Write(return2, 0.0f);

    // Read staggered, signed taps from both sides of the tank.
    float left;
    float right;
    c.Read(delay1, 127, 0.45f);
    c.Read(delay1, 2153, 0.35f);
    c.Read(decay2, 907, -0.25f);
    c.Read(return2, 3109, 0.3f);
    c.Write(left, 0.0f);
    c.Read(delay2, 193, 0.45f);
    c.Read(delay2, 2741, 0.35f);
    c.Read(decay1, 1207, -0.25f);
    c.Read(return1, 3593, 0.3f);
    c.Write(right);

    in_out->l = in_out->l * dry + left * wet;
    in_out->r = in_out->r * dry + right * wet;
    ++in_out;
  }
}

}  // namespace beads

// Copyright 2026 Insolent Construct.
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

// Included in the Stock and Bloom renderers.

    // This is the Griesinger topology described in the Dattorro paper
    // (4 AP diffusers on the input, then a loop of 2x 2AP+1Delay).
    // Modulation is applied in the loop of the first diffuser AP for additional
    // smearing; and to the two long delays for a slow shimmer/chorus effect.
    typedef E::Reserve<150,
      E::Reserve<214,
      E::Reserve<319,
      E::Reserve<527,
      E::Reserve<2182,
      E::Reserve<2690,
      E::Reserve<4501,
      E::Reserve<2525,
      E::Reserve<2197,
      E::Reserve<6312> > > > > > > > > > Memory;
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

#ifndef BEADS_REVERB_BLOOM
    const float kap = 0.625f;
#endif  // BEADS_REVERB_BLOOM
    const float klp = lp_;
#ifdef BEADS_REVERB_BLOOM
    const float decay = reverb_time_;
#else
    const float krt = reverb_time_;
#endif  // BEADS_REVERB_BLOOM

    const float input_gain = 0.2f * (1.0f - 0.333f * reverb_time_);
    const float output_gain = 0.4f / input_gain;

    // const float send_amount = 1.0f;
    // const float wet = mix_ * (2.0f - mix_) * output_gain;

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
      c.Interpolate(ap1, 10.0f, LFO_1, 80.0f, 1.0f);

      // The other reads don't alias this sample's writes: load them first.
      const float ap1_tail = c.Tail(ap1);
      const float ap2_tail = c.Tail(ap2);
      const float ap3_tail = c.Tail(ap3);
      const float ap4_tail = c.Tail(ap4);
      const float del2_tap = c.Tap(del2, 6261.0f, LFO_2, 50.0f);
      const float dap1a_tail = c.Tail(dap1a);
      const float dap1b_tail = c.Tail(dap1b);
      const float del1_tap = c.Tap(del1, 4460.0f, LFO_1, 40.0f);
      const float dap2a_tail = c.Tail(dap2a);
      const float dap2b_tail = c.Tail(dap2b);

      c.Write(ap1, 100, 0.0f);

#ifdef BEADS_REVERB_BLOOM
      const float input = in_out->l + in_out->r;
      const float density = state.Density(fabsf(input) * send_amount);
      const float kap = 0.625f * (0.3f + 0.7f * density);
      const float krt = decay * (0.55f + 0.45f * density);
      c.Read(input, send_amount);
#else
      c.Read(in_out->l + in_out->r, send_amount);
#endif  // BEADS_REVERB_BLOOM

      // Diffuse through 4 allpasses.
      c.ReadTail(ap1_tail, kap);
      c.WriteAllPass(ap1, -kap);
      c.ReadTail(ap2_tail, kap);
      c.WriteAllPass(ap2, -kap);
      c.ReadTail(ap3_tail, kap);
      c.WriteAllPass(ap3, -kap);
      c.ReadTail(ap4_tail, kap);
      c.WriteAllPass(ap4, -kap);
      c.Write(apout);

      // Main reverb loop.
      c.Load(apout);
      c.Read(del2_tap, krt);
      c.Lp(lp_1_, klp);
      c.ReadTail(dap1a_tail, -kap);
      c.WriteAllPass(dap1a, kap);
      c.ReadTail(dap1b_tail, kap);
      c.WriteAllPass(dap1b, -kap);
      c.Write(del1, 1.0f);
      c.Write(wet);

      in_out->l = in_out->l * dry_amount + wet * wet_amount;

      c.Load(apout);
      c.Read(del1_tap, krt);
      c.Lp(lp_2_, klp);
      c.ReadTail(dap2a_tail, kap);
      c.WriteAllPass(dap2a, -kap);
      c.ReadTail(dap2b_tail, -kap);
      c.WriteAllPass(dap2b, kap);
      c.Write(del2, 1.0f);
      c.Write(wet);

      in_out->r = in_out->r * dry_amount + wet * wet_amount;
      ++in_out;
    }

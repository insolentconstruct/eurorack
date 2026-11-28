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
//
// Parasites Oliverb, adapted for Beads' native codec rates.

#ifndef BEADS_DSP_FX_OLIVERB_H_
#define BEADS_DSP_FX_OLIVERB_H_

#include "stmlib/dsp/units.h"

#include "beads/dsp/fx/fx_engine.h"
#include "beads/dsp/frame.h"
#include "beads/resources.h"

namespace beads {

class Oliverb {
 public:
  struct Controls {
    float pre_delay;
    float size;
    float pitch;
    float decay;
    float texture;
    float diffusion;
    float mod_rate;
    float mod_depth;
    float mix;
  };

  Oliverb() { }
  ~Oliverb() { }

  inline int sample_rate() const { return sample_rate_; }

  void Init(uint16_t* buffer, int sample_rate) {
    sample_rate_ = sample_rate;
    switch (sample_rate_) {
      case 24000:
        render_ = &Oliverb::Render<3, 4>;
        break;
      case 32000:
        render_ = &Oliverb::Render<1, 1>;
        break;
      default:
        render_ = &Oliverb::Render<3, 2>;
        break;
    }
    engine_.Init(buffer);
    engine_.SetLFOFrequency(LFO_1, 0.0f);
    engine_.SetLFOFrequency(LFO_2, 0.0f);
    smooth_size_ = 0.0f;
    pre_delay_ = 0.018f * float(sample_rate_);
    pitch_phase_ = 0.0f;
    lp_1_ = lp_2_ = hp_1_ = hp_2_ = 0.0f;
    random_state_ = 1;
    for (size_t i = 0; i < 9; ++i) {
      lfo_[i].Init(Random());
    }
  }

  void Process(const Controls& controls, const bool* freeze,
      const FloatFrame* in, FloatFrame* out, size_t size) {
    (this->*render_)(controls, freeze, in, out, size);
  }

 private:
  typedef FxEngine<32768, FORMAT_16_BIT> E;

  static float RaisedCosine(float phase) {
    return 0.5f - 0.5f *
        stmlib::Interpolate(lut_sine + 256, phase, 512.0f);
  }

  // Smoothed random oscillator from Parasites (Matthias Puech, MIT).
  class RandomOscillator {
   public:
    void Init(float random) {
      phase_ = 0.0f;
      value_ = 0.0f;
      target_ = 2.0f * random - 1.0f;
      rising_ = false;
    }

    void SetSlope(float slope) {
      increment_ = std::min(1.0f, (1.0f / fabsf(target_ - value_)) * slope);
    }

    float Next(Oliverb* reverb) {
      phase_ += increment_;
      if (phase_ > 1.0f) {
        phase_ -= 1.0f;
        value_ = target_;
        rising_ = !rising_;
        const float distance = 0.7f * reverb->Random() + 0.3f;
        target_ = rising_ ? value_ + (1.0f - value_) * distance
                          : value_ - (1.0f + value_) * distance;
      }
      return value_ + (target_ - value_) * RaisedCosine(phase_);
    }

   private:
    float phase_;
    float increment_;
    float value_;
    float target_;
    bool rising_;
  };

  float Random() {
    random_state_ = random_state_ * 1664525L + 1013904223L;
    return float(random_state_) / 4294967296.0f;
  }

  template<typename D>
  void Read(E::Context* c, D& delay, float modulation, float gain) {
    float offset = (D::length - 1) * smooth_size_ + modulation;
    CONSTRAIN(offset, 1.0f, D::length - 1);
    c->InterpolateHermite(delay, offset, gain);
  }

  template<int numerator, int denominator>
  static float ScaleCoefficient(float coefficient) {
    if (numerator == denominator) {
      return coefficient;
    }
    // Raise the pole to 2/3 at 48 kHz, or 4/3 at 24 kHz.
    const float pole = 1.0f - coefficient;
    float root = 0.5f + 0.5f * pole;
    for (int i = 0; i < 4; ++i) {
      root = (2.0f * root + pole / (root * root)) * (1.0f / 3.0f);
    }
    return 1.0f - (denominator == 2 ? root * root : pole * root);
  }

  template<int numerator, int denominator>
  void Render(const Controls& controls, const bool* freeze,
      const FloatFrame* in, FloatFrame* out, size_t size) {
    typedef E::Reserve<113 * numerator / denominator,
      E::Reserve<162 * numerator / denominator,
      E::Reserve<241 * numerator / denominator,
      E::Reserve<399 * numerator / denominator,
      E::Reserve<1253 * numerator / denominator,
      E::Reserve<1738 * numerator / denominator,
      E::Reserve<3411 * numerator / denominator,
      E::Reserve<1513 * numerator / denominator,
      E::Reserve<1363 * numerator / denominator,
      E::Reserve<4782 * numerator / denominator,
      E::Reserve<4802 * numerator / denominator> > > > > > > > > > > Memory;
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
    E::DelayLine<Memory, 10> pre_delay;
    E::Context c;

    const float scale = float(numerator) / float(denominator);
    const float size_target = 0.05f + 0.94f * controls.size;
    const float delay_target = controls.pre_delay * sample_rate_;
    const float smoothing = ScaleCoefficient<numerator, denominator>(0.01f);
    const float diffusion = 0.3f + 0.5f * controls.diffusion;
    const float depth = controls.mod_depth * 300.0f * scale;
    const float abs_pitch = fabsf(controls.pitch);
    const float ratio = stmlib::SemitonesToRatio(controls.pitch);
    float shift = (abs_pitch - 0.3f) / 0.4f;
    CONSTRAIN(shift, 0.0f, 1.0f);
    const float decay = controls.decay * 1.3f + 0.15f * abs_pitch / 24.0f;
    const float lowpass = 0.03f + 0.9f *
        std::min(1.0f, 2.0f * controls.texture);
    const float highpass = 0.01f + 0.2f *
        std::max(0.0f, 2.0f * controls.texture - 1.0f);
    const float lp = ScaleCoefficient<numerator, denominator>(lowpass);
    const float hp = ScaleCoefficient<numerator, denominator>(highpass);
    float slope = controls.mod_rate * controls.mod_rate;
    slope *= slope * slope;
    slope /= 200.0f * scale;
    for (size_t i = 1; i < 9; ++i) {
      lfo_[i].SetSlope(slope);
    }
    const float dry = stmlib::Interpolate(
        lut_sine, 1.0f - controls.mix, 256.0f);
    const float wet = 0.42426407f *
        stmlib::Interpolate(lut_sine, controls.mix, 256.0f);

    for (size_t i = 0; i < size; ++i) {
      engine_.Start(&c);
      ONE_POLE(smooth_size_, size_target, smoothing);
      ONE_POLE(pre_delay_, delay_target, smoothing);
      const float window = (128.0f + (3410.0f - 128.0f) * smooth_size_) * scale;
      pitch_phase_ += (1.0f - ratio) / window;
      if (pitch_phase_ >= 1.0f) {
        pitch_phase_ -= 1.0f;
      }
      if (pitch_phase_ <= 0.0f) {
        pitch_phase_ += 1.0f;
      }
      const float triangle = 2.0f * std::min(pitch_phase_, 1.0f - pitch_phase_);
      const float crossfade = RaisedCosine(triangle);
      const float phase = pitch_phase_ * window;
      float half = phase + window * 0.5f;
      if (half >= window) {
        half -= window;
      }

      // Parasites leaves the diffuser's fixed LFO at zero.
      c.Interpolate(ap1, 10.0f * scale, 1.0f);
      c.Write(ap1, 100 * numerator / denominator, 0.0f);

      const float mono = (in[i].l + in[i].r) * 0.5f;
      c.Load(freeze[i] ? 0.0f : mono);
      c.Write(pre_delay, 0.0f);
      if (!freeze[i]) {
        c.Interpolate(pre_delay, pre_delay_, 1.0f);
      }
      Read(&c, ap1, lfo_[1].Next(this) * depth, diffusion);
      c.WriteAllPass(ap1, -diffusion);
      Read(&c, ap2, lfo_[2].Next(this) * depth, diffusion);
      c.WriteAllPass(ap2, -diffusion);
      Read(&c, ap3, lfo_[3].Next(this) * depth, diffusion);
      c.WriteAllPass(ap3, -diffusion);
      Read(&c, ap4, lfo_[4].Next(this) * depth, diffusion);
      c.WriteAllPass(ap4, -diffusion);

      float diffuse;
      c.Write(diffuse);
      const float loop = freeze[i] ? 1.0f : decay;
      const float direct = loop * (1.0f - shift);
      const float shifted = loop * shift;
      const float lowpass_coefficient = freeze[i] ? 1.0f : lp;

      Read(&c, del2, lfo_[5].Next(this) * depth, direct);
      c.InterpolateHermite(del2, phase, crossfade * shifted);
      c.InterpolateHermite(del2, half, (1.0f - crossfade) * shifted);
      c.Lp(lp_1_, lowpass_coefficient);
      if (freeze[i]) {
        hp_1_ = 0.0f;
      } else {
        c.Hp(hp_1_, hp);
      }
      c.SoftLimit();
      Read(&c, dap1a, lfo_[6].Next(this) * depth, -diffusion);
      c.WriteAllPass(dap1a, diffusion);
      Read(&c, dap1b, 0.0f, diffusion);
      c.WriteAllPass(dap1b, -diffusion);
      c.Write(del1, 2.0f);
      float left;
      c.Write(left);

      c.Load(diffuse);
      Read(&c, del1, lfo_[7].Next(this) * depth, direct);
      c.InterpolateHermite(del1, phase, crossfade * shifted);
      c.InterpolateHermite(del1, half, (1.0f - crossfade) * shifted);
      c.Lp(lp_2_, lowpass_coefficient);
      if (freeze[i]) {
        hp_2_ = 0.0f;
      } else {
        c.Hp(hp_2_, hp);
      }
      c.SoftLimit();
      Read(&c, dap2a, lfo_[8].Next(this) * depth, diffusion);
      c.WriteAllPass(dap2a, -diffusion);
      Read(&c, dap2b, 0.0f, -diffusion);
      c.WriteAllPass(dap2b, diffusion);
      c.Write(del2, 2.0f);
      float right;
      c.Write(right);

      out[i].l = dry * in[i].l + wet * left;
      out[i].r = dry * in[i].r + wet * right;
    }
  }

  E engine_;
  void (Oliverb::*render_)(const Controls&, const bool*,
      const FloatFrame*, FloatFrame*, size_t);
  int sample_rate_;
  float smooth_size_;
  float pre_delay_;
  float pitch_phase_;
  float lp_1_;
  float lp_2_;
  float hp_1_;
  float hp_2_;
  uint32_t random_state_;
  RandomOscillator lfo_[9];

  DISALLOW_COPY_AND_ASSIGN(Oliverb);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_OLIVERB_H_

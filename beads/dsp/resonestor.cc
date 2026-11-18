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
// Resonestor, from Matthias Puech's Parasites firmware for Clouds.

#include "beads/dsp/resonestor.h"

#include <cmath>
#include <cstring>

#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace beads {

using namespace stmlib;

static const float chords[3][18] = {
  { 0.0f, 4.0f/128.0f, 16.0f/128.0f, 4.0f/128.0f, 4.0f/128.0f,
    12.0f, 12.0f, 4.0f, 4.0f,
    3.0f, 3.0f, 2.0f, 4.0f,
    3.0f, 4.0f, 3.0f, 4.0f,
    4.0f },
  { 0.0f, 8.0f/128.0f, 32.0f/128.0f, 7.0f, 12.0f,
    24.0f, 7.0f, 7.0f, 7.0f,
    7.0f, 7.0f, 7.0f, 7.0f,
    7.0f, 7.0f, 7.0f, 7.0f,
    7.0f },
  { 0.0f, 12.0f/128.0f, 48.0f/128.0f, 7.0f + 4.0f/128.0f, 12.0f + 4.0f/128.0f,
    36.0f, 19.0f, 12.0f, 11.0f,
    10.0f, 12.0f, 12.0f, 12.0f,
    14.0f, 14.0f, 16.0f, 16.0f,
    16.0f }
};

const float kPlateau = 2.0f;
const float kBasePitch = 261.626f;

inline float InterpolatePlateau(const float* table, float index, float size) {
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  float a = table[index_integral];
  float b = table[index_integral + 1];
  if (index_fractional < 1.0f / kPlateau) {
    return a + (b - a) * index_fractional * kPlateau;
  } else {
    return b;
  }
}

// Mantissa by a quadratic, within 0.005: enough for a comb's decay.
inline float Log2(float x) {
  int32_t bits;
  memcpy(&bits, &x, sizeof(bits));
  const float exponent = float(((bits >> 23) & 255) - 128);
  bits = (bits & 0x807fffff) | 0x3f800000;
  float m;
  memcpy(&m, &bits, sizeof(m));
  return exponent + (-0.34484843f * m + 2.02466578f) * m + 0.32512241f;
}

void Resonestor::Init(float* buffer, float sample_rate) {
  engine_.Init(buffer);
  sample_rate_ = sample_rate;
  for (int v = 0; v < 2; v++) {
    pitch_[v] = 0.0f;
    chord_[v] = 0.0f;
    feedback_[v] = 0.0f;
    narrow_[v] = 0.001f;
    damp_[v] = 1.0f;
    harmonicity_[v] = 1.0f;
    distortion_[v] = 0.0f;
  }
  spread_amount_ = 0.0f;
  stereo_ = 0.0f;
  separation_ = 0.0f;
  burst_time_ = 0.0f;
  burst_comb_ = 1.0f;
  burst_duration_ = 0.0f;
  trigger_ = previous_trigger_ = false;
  freeze_ = previous_freeze_ = false;
  voice_ = 0;
  for (int i = 0; i < 3; i++) {
    spread_delay_[i] = Random::GetFloat() * 3999;
  }
  burst_lp_.Init();
  rand_lp_.Init();
  rand_hp_.Init();
  rand_hp_.set_f<FREQUENCY_FAST>(1.0f / sample_rate_);
  for (int v = 0; v < 2; v++) {
    for (int p = 0; p < 4; p++) {
      lp_[p][v].Init();
      bp_[p][v].Init();
      hp_[p][v] = 0.0f;
      comb_period_[p][v] = 0.0f;
      comb_feedback_[p][v] = 0.0f;
    }
  }
}

void Resonestor::Process(FloatFrame* in_out, size_t size) {
  typedef E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb,
    E::Reserve<300,  /* bc */
    E::Reserve<6000,  /* bd0 */
    E::Reserve<6000,  /* bd1 */
    E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb,
    E::Reserve<kResonestorMaxComb> > > > > > > > > > > Memory;
  E::DelayLine<Memory, 0> c00;
  E::DelayLine<Memory, 1> c10;
  E::DelayLine<Memory, 2> c20;
  E::DelayLine<Memory, 3> c30;
  E::DelayLine<Memory, 4> bc;
  E::DelayLine<Memory, 5> bd0;
  E::DelayLine<Memory, 6> bd1;
  E::DelayLine<Memory, 7> c01;
  E::DelayLine<Memory, 8> c11;
  E::DelayLine<Memory, 9> c21;
  E::DelayLine<Memory, 10> c31;
  E::Context c;

  // Switch the active voice.
  if (trigger_ && !previous_trigger_ && !freeze_) {
    voice_ = !voice_;
  }

  if (freeze_ && !previous_freeze_) {
    previous_freeze_ = freeze_;
    voice_ = !voice_;
  }

  // Comb filter pitches.
  const float max_comb = float(kResonestorMaxComb);
  comb_period_[0][voice_] = sample_rate_ / kBasePitch / \
      SemitonesToRatio(pitch_[voice_]);
  CONSTRAIN(comb_period_[0][voice_], 0, max_comb);
  for (int p = 1; p < 4; p++) {
    float pitch = InterpolatePlateau(chords[p - 1], chord_[voice_], 16);
    comb_period_[p][voice_] = comb_period_[0][voice_] / \
        SemitonesToRatio(pitch);
    CONSTRAIN(comb_period_[p][voice_], 0, max_comb);
  }

  // LP/BP filter frequencies and feedback; libm's powf would need errno.
  const float log2_feedback = Log2(feedback_[voice_]);
  for (int p = 0; p < 4; p++) {
    float freq = 1.0f / comb_period_[p][voice_];
    bp_[p][voice_].set_f_q<FREQUENCY_FAST>(freq, narrow_[voice_]);
    float lp_freq = (2.0f * freq + 1.0f) * damp_[voice_];
    CONSTRAIN(lp_freq, 0.0f, 1.0f);
    lp_[p][voice_].set_f_q<FREQUENCY_FAST>(lp_freq, 0.4f);
    comb_feedback_[p][voice_] = feedback_[voice_] > 0.0f
        ? Exp2Safe(log2_feedback * comb_period_[p][voice_] / sample_rate_)
        : 0.0f;
  }

  // A trigger starts a burst.
  if (trigger_ && !previous_trigger_) {
    previous_trigger_ = trigger_;
    burst_time_ = comb_period_[0][voice_];
    burst_time_ *= 2.0f * burst_duration_;

    for (int i = 0; i < 3; i++) {
      spread_delay_[i] = Random::GetFloat() * (bd0.length - 1);
    }
  }

  rand_lp_.set_f_q<FREQUENCY_FAST>(distortion_[voice_] * 0.4f, 1.0f);

  // Control-rate terms, once per block.
  const float comb_fb = 0.6f - burst_comb_ * 0.4f;
  float comb_del = burst_comb_ * bc.length;
  if (comb_del <= 1.0f) {
    comb_del = 1.0f;
  }
  float amplitude = 1.0f - distortion_[voice_];
  amplitude *= 0.3f;
  amplitude *= amplitude;
  const float hp = 10.0f / sample_rate_;
  const float n0 = 1.0f + 0.5f * narrow_[0];
  const float n1 = 1.0f + 0.5f * narrow_[1];
  const float wide = 0.25f + 0.25f * stereo_;
  const float narrow = 0.25f * (1.0f - stereo_);
  const float together = 1.0f - separation_;

  while (size--) {
    engine_.Start(&c);

    burst_time_--;
    float burst_gain = burst_time_ > 0.0f ? 1.0f : 0.0f;

    float random = Random::GetFloat() * 2.0f - 1.0f;
    // Burst noise through a comb and a low-pass.
    c.Read(random, burst_gain);
    c.InterpolateHermite(bc, comb_del, comb_fb);
    c.Write(bc, 1.0f);
    float burst;
    c.Write(burst);
    c.Load(burst_lp_.Process<FILTER_MODE_LOW_PASS>(burst));
    c.Write(burst);

    c.Load(burst);
    c.Read(in_out->l, 1.0f);
    c.Write(bd0, 0.0f);

    c.Load(burst);
    c.Read(in_out->r, 1.0f);
    c.Write(bd1, 0.0f);

    random *= amplitude;
    random = rand_lp_.Process<FILTER_MODE_LOW_PASS>(random);
    random = rand_hp_.Process<FILTER_MODE_HIGH_PASS>(random);

#define COMB(pre, part, voice, vol) \
    { \
      c.Load(0.0f); \
      c.Read(bd ## voice, pre * spread_amount_, vol); \
      float tap = comb_period_[part][voice] * (1.0f + random); \
      c.InterpolateHermite(c ## part ## voice, tap, \
          comb_feedback_[part][voice] * 0.7f); \
      c.InterpolateHermite(c ## part ## voice, tap * harmonicity_[voice], \
          comb_feedback_[part][voice] * 0.3f); \
      float acc; \
      c.Write(acc); \
      acc = lp_[part][voice].Process<FILTER_MODE_LOW_PASS>(acc); \
      acc = bp_[part][voice].Process<FILTER_MODE_BAND_PASS_NORMALIZED>(acc); \
      c.Load(acc); \
      c.Hp(hp_[part][voice], hp); \
      c.Write(acc, 0.5f); \
      c.SoftLimit(); \
      c.Write(acc, 2.0f); \
      c.Write(c ## part ## voice, 0.0f); \
    }

    // First voice.
    COMB(0, 0, 0, !voice_);
    COMB(spread_delay_[0], 1, 0, !voice_);
    COMB(spread_delay_[1], 2, 0, !voice_);
    COMB(spread_delay_[2], 3, 0, !voice_);

    // Second voice.
    COMB(0, 0, 1, voice_);
    COMB(spread_delay_[0], 1, 1, voice_);
    COMB(spread_delay_[1], 2, 1, voice_);
    COMB(spread_delay_[2], 3, 1, voice_);
#undef COMB

    // Left mix.
    c.Read(c00, n0 * narrow * together);
    c.Read(c10, n0 * wide * together);
    c.Read(c20, n0 * narrow * together);
    c.Read(c30, n0 * wide * together);
    c.Read(c01, n1 * wide);
    c.Read(c11, n1 * narrow);
    c.Read(c21, n1 * wide);
    c.Read(c31, n1 * narrow);
    c.Write(in_out->l, 0.0f);

    // Right mix.
    c.Read(c00, n0 * wide);
    c.Read(c10, n0 * narrow);
    c.Read(c20, n0 * wide);
    c.Read(c30, n0 * narrow);
    c.Read(c01, n1 * narrow * together);
    c.Read(c11, n1 * wide * together);
    c.Read(c21, n1 * narrow * together);
    c.Read(c31, n1 * wide * together);
    c.Write(in_out->r, 0.0f);

    ++in_out;
  }
}

}  // namespace beads

// Copyright 2012 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
// Fluted: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/fluted_engine.h"

#include "beads/blob/models/palette_clear_buffer.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float ApplyMacro(float stock_value, float minimum_value,
    float maximum_value, float macro) {
  const float amount = macro * 2.0f;
  return macro < 0.5f
      ? minimum_value + (stock_value - minimum_value) * amount
      : stock_value + (maximum_value - stock_value) * (amount - 1.0f);
}

}  // namespace

namespace {

// lut_flute_body_filter, verbatim from braids/resources.cc, first 80 entries.
const uint16_t kFluteBodyFilter[80] = {
     30,    32,    34,    36,    38,    40,    43,    45,
     48,    51,    54,    57,    60,    64,    68,    72,
     76,    81,    86,    91,    96,   102,   108,   114,
    121,   129,   136,   144,   153,   162,   172,   182,
    193,   204,   216,   229,   243,   258,   273,   289,
    306,   325,   344,   364,   386,   409,   433,   459,
    487,   516,   546,   579,   613,   650,   688,   729,
    773,   819,   867,   919,   974,  1032,  1093,  1158,
   1227,  1300,  1377,  1459,  1546,  1638,  1735,  1839,
   1948,  2064,  2187,  2317,  2454,  2600,  2755,  2867
};

// 31-tap Kaiser(beta = 6) halfband, 96 -> 48 kHz, the same design blown_engine
const float kHalfbandCentre = 0.4999739547f;
const float kHalfband[8] = {
   0.3144409659f,
  -0.0949999614f,
   0.0465914822f,
  -0.0242523503f,
   0.0119896864f,
  -0.0052090058f,
   0.0017678112f,
  -0.0003156056f
};

// lut_blowing_envelope, evaluated from its generator (lookup_tables.py:150-156)
inline int BlowingEnvelope(int index) {
  if (index < kFlutedEnvelopeAttack) {
    return (index * 212992) / 1190;
  }
  int i = index - kFlutedEnvelopeAttack;
  if (i > kFlutedEnvelopeDecay - 1) {
    i = kFlutedEnvelopeDecay - 1;
  }
  return (509050880 - 425984 * i) / 23900;
}

// Braids' lut_blowing_jet verbatim. The former closed-form evaluator produced
const int16_t kBlowingJet[257] = {
       0,   -255,   -511,   -767,
   -1022,  -1278,  -1532,  -1786,
   -2039,  -2292,  -2544,  -2795,
   -3044,  -3293,  -3541,  -3787,
   -4031,  -4275,  -4516,  -4756,
   -4994,  -5231,  -5465,  -5697,
   -5927,  -6155,  -6381,  -6604,
   -6824,  -7042,  -7257,  -7470,
   -7679,  -7886,  -8089,  -8289,
   -8486,  -8680,  -8870,  -9056,
   -9239,  -9418,  -9594,  -9765,
   -9932, -10095, -10254, -10409,
  -10559, -10705, -10846, -10982,
  -11114, -11241, -11363, -11480,
  -11591, -11698, -11799, -11894,
  -11984, -12069, -12147, -12220,
  -12287, -12348, -12403, -12452,
  -12494, -12530, -12560, -12583,
  -12599, -12609, -12611, -12607,
  -12596, -12578, -12552, -12519,
  -12479, -12431, -12376, -12313,
  -12242, -12163, -12077, -11982,
  -11879, -11768, -11649, -11521,
  -11384, -11239, -11085, -10923,
  -10751, -10571, -10381, -10182,
   -9974,  -9757,  -9530,  -9293,
   -9047,  -8791,  -8526,  -8250,
   -7964,  -7668,  -7362,  -7046,
   -6719,  -6382,  -6034,  -5676,
   -5306,  -4926,  -4535,  -4133,
   -3719,  -3295,  -2859,  -2411,
   -1952,  -1482,  -1000,   -506,
       0,    517,   1048,   1590,
    2144,   2711,   3291,   3883,
    4487,   5105,   5735,   6378,
    7034,   7704,   8386,   9082,
    9791,  10514,  11250,  12000,
   12764,  13542,  14333,  15139,
   15959,  16793,  17642,  18504,
   19382,  20274,  21181,  22102,
   23039,  23990,  24957,  25939,
   26936,  27948,  28976,  30019,
   31079,  32153,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,  32767,  32767,  32767,
   32767,
};

inline int BlowingJet(int index) {
  return kBlowingJet[index >> 8];
}

// stmlib::Random's generator, so the breath noise has Braids' distribution
inline int NextNoise(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return static_cast<int>(static_cast<int16_t>(*state >> 16));
}

// stmlib::Mix(a, b, balance) = `(a * (65535 - balance) + b * balance) >> 16`
inline int MixLine(
    const int8_t* line, uint32_t mask, uint32_t read, uint32_t balance) {
  const int a = static_cast<int>(line[read & mask]);
  const int b = static_cast<int>(line[(read - 1) & mask]);
  return (a * static_cast<int>(65535u - balance) +
          b * static_cast<int>(balance)) >> 16;
}

// Braids' `lut_flute_body_filter[pitch_ >> 7]` (1404) with the out-of-bounds
inline int BodyCoefficient(float note) {
  int pitch = static_cast<int>(note * 128.0f);
  CONSTRAIN(pitch, 0, 140 * 128);
  int index = pitch >> 7;
  CONSTRAIN(index, 0, 79);
  return static_cast<int>(kFluteBodyFilter[index]);
}

inline void ResolveFlutedDelays(
    float frequency,
    int color_code,
    uint32_t* bore_delay_integral,
    uint32_t* bore_delay_fractional,
    uint32_t* jet_delay_integral,
    uint32_t* jet_delay_fractional,
    uint32_t* pickup_integral,
    uint32_t* pickup_fractional) {
  const uint32_t delay_fixed = static_cast<uint32_t>(
      min(2.0f / max(1e-6f, frequency), 16383.0f) * 65536.0f);
  uint32_t bore_delay = (delay_fixed << 1) - (2u << 16);
  uint32_t jet_delay = (bore_delay >> 8) *
      static_cast<uint32_t>(48 + (color_code >> 10));
  bore_delay -= jet_delay;
  int guard = 0;
  while (guard < 24 &&
         (bore_delay > ((kFlutedBoreLength - 1) << 16) ||
          jet_delay > ((kFlutedJetLength - 1) << 16))) {
    bore_delay >>= 1;
    jet_delay >>= 1;
    ++guard;
  }
  *bore_delay_integral = bore_delay >> 16;
  *bore_delay_fractional = bore_delay & 0xffff;
  *jet_delay_integral = jet_delay >> 16;
  *jet_delay_fractional = jet_delay & 0xffff;

  uint32_t pickup = static_cast<uint32_t>(
      static_cast<float>(bore_delay) * kFlutedAuxPickup);
  if (pickup < (2u << 16)) {
    pickup = 2u << 16;
  }
  *pickup_integral = pickup >> 16;
  *pickup_fractional = pickup & 0xffff;
}

}  // namespace

void Fluted::Init(BufferAllocator* allocator) {
  bore_ = allocator->Allocate<int8_t>(kFlutedBoreLength);
  jet_ = allocator->Allocate<int8_t>(kFlutedJetLength);
  Reset();
}

void Fluted::Reset() {
  // DigitalOscillator::Init() zeroes state_ and Render() calls it on every
  if (bore_) {
    ClearBuffer(bore_, kFlutedBoreLength * sizeof(bore_[0]));
  }
  if (jet_) {
    ClearBuffer(jet_, kFlutedJetLength * sizeof(jet_[0]));
  }
  delay_pointer_ = 0;
  lp_state_ = 0;
  dc_blocking_x0_ = 0;
  dc_blocking_y0_ = 0;
  excitation_pointer_ = 0;
  blowing_envelope_ = BlowingEnvelope(excitation_pointer_);
  excitation_divider_ = 0;
  noise_depth_ = 2100.0f;
  blow_ = kFlutedBlowStock;
  rng_state_ = 0x4b1d07a3u;
  for (size_t i = 0; i < 32; ++i) {
    history_[i] = 0.0f;
    history_aux_[i] = 0.0f;
  }
  history_write_ = 0;
  out_dc_input_ = 0.0f;
  out_dc_output_ = 0.0f;
  aux_dc_input_ = 0.0f;
  aux_dc_output_ = 0.0f;
}

void Fluted::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (!bore_ || !jet_) {
    // Allocate<T> returns NULL silently when the arena is exhausted (R15).
    for (size_t i = 0; i < size; ++i) {
      out[i] = 0.0f;
      aux[i] = 0.0f;
    }
    return;
  }

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    // Braids' strike_ handling, 1381-1387: restart the excitation envelope,
    ClearBuffer(bore_, kFlutedBoreLength * sizeof(bore_[0]));
    ClearBuffer(jet_, kFlutedJetLength * sizeof(jet_[0]));
    lp_state_ = 0;
    excitation_pointer_ = 0;
    blowing_envelope_ = BlowingEnvelope(excitation_pointer_);
    excitation_divider_ = 0;
  }

  // R5: the internal rate is 96 kHz, two steps per output sample, so Braids'
  float delay_note = parameters.note;
  CONSTRAIN(delay_note, 0.0f, 128.0f);
  const float frequency = max(1e-6f, NoteToFrequency(delay_note));
  const int color_code = static_cast<int>(parameters.harmonics * 32767.0f);
  uint32_t bore_delay_integral;
  uint32_t bore_delay_fractional;
  uint32_t jet_delay_integral;
  uint32_t jet_delay_fractional;
  uint32_t pickup_integral;
  uint32_t pickup_fractional;
  ResolveFlutedDelays(
      frequency, color_code,
      &bore_delay_integral, &bore_delay_fractional,
      &jet_delay_integral, &jet_delay_fractional,
      &pickup_integral, &pickup_fractional);

  // Line 1403: breath_intensity = 2100 - (TIMBRE >> 4), applied at 1421 as a
  const int timbre_code = static_cast<int>(parameters.timbre * 32767.0f);
  const float target_noise_depth =
      2100.0f - static_cast<float>(timbre_code >> 4);

  // The two macros Braids does not have, both stock at noon. ApplyMacro
  const float target_blow = ApplyMacro(
      kFlutedBlowStock, kFlutedBlowMin, kFlutedBlowMax, parameters.morph);
  const float body_offset = ApplyMacro(
      kFlutedBodyStock, kFlutedBodyMin, kFlutedBodyMax, 0.5f);

  // The reflection filter's corner. Braids derives it from the note alone and
  const int body_coefficient = BodyCoefficient(parameters.note + body_offset);
  const int body_complement = 4096 - body_coefficient;

  ParameterInterpolator noise_modulation(
      &noise_depth_, target_noise_depth, size);
  ParameterInterpolator blow_modulation(&blow_, target_blow, size);

  const uint32_t bore_mask = kFlutedBoreLength - 1;
  const uint32_t jet_mask = kFlutedJetLength - 1;
  // The envelope is piecewise constant between pointer advances. Preserve its
  int blowing_envelope = blowing_envelope_;

  while (size--) {
    // ROUND, do not truncate. Braids' `breath_intensity` is an exact integer
    const int breath_intensity =
        static_cast<int>(noise_modulation.Next() + 0.5f);
    const int blow_code =
        static_cast<int>(blow_modulation.Next() * 4096.0f + 0.5f);

    for (int j = 0; j < 2; ++j) {
      // MixLine stays in [-128, 127]; multiplication preserves negative samples.
      const int32_t bore_value = MixLine(
          bore_, bore_mask,
          static_cast<uint32_t>(
              delay_pointer_ + 2 * kFlutedBoreLength - bore_delay_integral),
          bore_delay_fractional) * 512;
      const int32_t jet_value = MixLine(
          jet_, jet_mask,
          static_cast<uint32_t>(
              delay_pointer_ + 2 * kFlutedJetLength - jet_delay_integral),
          jet_delay_fractional) * 512;

      // The breath: the excitation envelope doubled (1419-1420), with
      int32_t breath_pressure = blowing_envelope << 1;
      breath_pressure = breath_pressure * blow_code >> 12;
      int32_t random_pressure =
          NextNoise(&rng_state_) * breath_intensity >> 12;
      random_pressure = random_pressure * breath_pressure >> 15;
      breath_pressure += random_pressure;

      // The inverting reflection one-pole, 1425-1426.
      lp_state_ = (-body_coefficient * bore_value +
                   body_complement * lp_state_) >> 12;

      // The in-loop DC blocker, 1428-1431.
      dc_blocking_y0_ = kFlutedDcBlockingPole * dc_blocking_y0_ >> 12;
      dc_blocking_y0_ += lp_state_ - dc_blocking_x0_;
      dc_blocking_x0_ = lp_state_;
      const int32_t reflection = dc_blocking_y0_;

      // The jet line takes the pressure difference at the embouchure, 1433.
      jet_[delay_pointer_ & jet_mask] =
          static_cast<int8_t>((breath_pressure - (reflection >> 1)) >> 9);

      // The jet characteristic, 1436-1445: the flow is clamped to 0..65535 --
      int32_t jet_table_index = jet_value;
      CONSTRAIN(jet_table_index, 0, 65535);
      const int32_t bore_input = BlowingJet(jet_table_index) + (reflection >> 1);

      // A second tap halfway along the bore, read before this sample's write
      const int32_t pickup_value = MixLine(
          bore_, bore_mask,
          static_cast<uint32_t>(
              delay_pointer_ + 2 * kFlutedBoreLength - pickup_integral),
          pickup_fractional) * 512;

      bore_[delay_pointer_ & bore_mask] = static_cast<int8_t>(bore_input >> 9);
      ++delay_pointer_;

      // Braids advances the excitation on three samples in four, because the
      if ((excitation_divider_ & 3u) != 3u &&
          excitation_pointer_ < kFlutedEnvelopePlateau) {
        ++excitation_pointer_;
        blowing_envelope = BlowingEnvelope(excitation_pointer_);
      }
      ++excitation_divider_;

      // The module's output, 1449-1451.
      int32_t sample = bore_value >> 1;
      CLIP(sample)

      int32_t aux_sample = pickup_value >> 1;
      CLIP(aux_sample)

      history_[history_write_ & 31] =
          static_cast<float>(sample) * (1.0f / 32768.0f);
      history_aux_[history_write_ & 31] =
          static_cast<float>(aux_sample) * (1.0f / 32768.0f);
      ++history_write_;
    }

    // 31-tap halfband, centred 15 internal samples behind the newest.
    const uint32_t centre = history_write_ - 16;
    float decimated = kHalfbandCentre * history_[centre & 31];
    float decimated_aux = kHalfbandCentre * history_aux_[centre & 31];
    for (int k = 0; k < 8; ++k) {
      const uint32_t offset = static_cast<uint32_t>(2 * k + 1);
      decimated += kHalfband[k] * (history_[(centre - offset) & 31] +
                                   history_[(centre + offset) & 31]);
      decimated_aux += kHalfband[k] * (history_aux_[(centre - offset) & 31] +
                                       history_aux_[(centre + offset) & 31]);
    }

    // The jet characteristic is read over a positive-biased range, so the bore
    ONE_POLE(out_dc_input_, decimated, kFlutedDcCoefficient);
    ONE_POLE(aux_dc_input_, decimated_aux, kFlutedDcCoefficient);
    out_dc_output_ = decimated - out_dc_input_;
    aux_dc_output_ = decimated_aux - aux_dc_input_;

    *out++ = out_dc_output_;
    *aux++ = aux_dc_output_;
  }
  blowing_envelope_ = blowing_envelope;
}

}  // namespace plaits

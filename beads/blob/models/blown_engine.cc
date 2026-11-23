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
// Blown: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/blown_engine.h"

#include "beads/blob/models/palette_clear_buffer.h"

#include <algorithm>

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

// lut_flute_body_filter, verbatim from braids/resources.cc:870, first 80
const uint16_t kBodyFilter[80] = {
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

// 31-tap Kaiser(beta = 6) halfband, 96 -> 48 kHz. Only the centre tap and the
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

// stmlib::Random's generator, so the breath noise has the same distribution as
inline float NextNoise(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return static_cast<float>(
      static_cast<int16_t>(*state >> 16)) * (1.0f / 32768.0f);
}

// Braids' body filter index: `(pitch_ - 8192 + (COLOR >> 1)) >> 7`, clamped to
inline float BodyCoefficient(float note, float harmonics) {
  int pitch = static_cast<int>(note * 128.0f);
  CONSTRAIN(pitch, 0, 140 * 128);
  const int color = static_cast<int>(harmonics * 32767.0f);
  int index = (pitch - 8192 + (color >> 1)) >> 7;
  CONSTRAIN(index, 0, 127);
  if (index > 79) {
    index = 79;
  }
  return static_cast<float>(kBodyFilter[index]) * (1.0f / 4096.0f);
}

}  // namespace

void Blown::Init(BufferAllocator* allocator) {
  bore_ = allocator->Allocate<float>(kBlownBoreLength);
  Reset();
}

void Blown::Reset() {
  // DigitalOscillator::Init() zeroes the whole state block and Render() calls
  if (bore_) {
    ClearBuffer(bore_, kBlownBoreLength * sizeof(bore_[0]));
  }
  delay_pointer_ = 0;
  lp_state_ = 0.0f;
  body_state_ = 0.0f;
  body_state_aux_ = 0.0f;
  noise_depth_ = kBlownBreathPressure;
  body_coefficient_ = 0.05f;
  blow_ = kBlownBreathPressure;
  reed_offset_ = kBlownReedOffset;
  rng_state_ = 0x8f2c1d3bu;
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

void Blown::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (!bore_) {
    // Allocate<T> returns NULL silently when the arena is exhausted (R15).
    for (size_t i = 0; i < size; ++i) {
      out[i] = 0.0f;
      aux[i] = 0.0f;
    }
    return;
  }

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    // Braids' strike_ handling flushes the bore and NOTHING else
    ClearBuffer(bore_, kBlownBoreLength * sizeof(bore_[0]));
  }

  const bool stereo = false;

  // R5: the internal rate is 96 kHz, two steps per output sample, so Braids'
  const float frequency = max(1e-6f, NoteToFrequency(parameters.note));
  float delay = 1.0f / frequency - 1.0f;
  // Braids' octave fold for bores too long for the line (1317-1319).
  int guard = 0;
  while (guard < 16 &&
         delay > static_cast<float>(kBlownBoreLength - 1)) {
    delay *= 0.5f;
    ++guard;
  }
  CONSTRAIN(delay, 4.0f, static_cast<float>(kBlownBoreLength - 2));

  const uint32_t base_delay_integral = static_cast<uint32_t>(delay);
  const float base_delay_fractional =
      delay - static_cast<float>(base_delay_integral);

  float pickup = delay * kBlownStereoPickup;
  CONSTRAIN(pickup, 2.0f, static_cast<float>(kBlownBoreLength - 2));
  const uint32_t base_pickup_integral = static_cast<uint32_t>(pickup);
  const float base_pickup_fractional =
      pickup - static_cast<float>(base_pickup_integral);

  // TIMBRE, line 1322: `28000 - (parameter_[0] >> 1)`, applied at 1334 as
  const int timbre_code = static_cast<int>(parameters.timbre * 32767.0f);
  const float target_noise_depth =
      (28000.0f - static_cast<float>(timbre_code >> 1)) * (1.0f / 32768.0f);
  const float target_body = BodyCoefficient(
      parameters.note, parameters.harmonics);

  // The two macros Braids does not have, both stock at noon.
  const float target_blow = ApplyMacro(
      kBlownBreathPressure, kBlownBlowMin, kBlownBlowMax, parameters.morph);
  const float target_reed_offset = ApplyMacro(
      kBlownReedOffset, kBlownReedMin, kBlownReedMax, 0.5f);

  ParameterInterpolator noise_modulation(
      &noise_depth_, target_noise_depth, size);
  ParameterInterpolator body_modulation(&body_coefficient_, target_body, size);
  ParameterInterpolator blow_modulation(&blow_, target_blow, size);
  ParameterInterpolator reed_modulation(
      &reed_offset_, target_reed_offset, size);

  size_t sample_index = 0;
  while (size--) {
    uint32_t delay_integral = base_delay_integral;
    float delay_fractional = base_delay_fractional;
    uint32_t pickup_integral = base_pickup_integral;
    float pickup_fractional = base_pickup_fractional;
    const float noise_depth = noise_modulation.Next();
    const float body_coefficient = body_modulation.Next();
    const float blow = blow_modulation.Next();
    const float reed_offset = reed_modulation.Next();

    for (int j = 0; j < 2; ++j) {
      // Breath: a static pressure multiplied by white noise, 1334-1336.
      const float breath = blow *
          (1.0f + NextNoise(&rng_state_) * noise_depth);

      // The fractional read, 1338-1342. delay_pointer_ is the NEXT write, so
      const uint32_t read =
          delay_pointer_ + 2 * kBlownBoreLength - delay_integral;
      const float a = bore_[read & (kBlownBoreLength - 1)];
      const float b = bore_[(read - 1) & (kBlownBoreLength - 1)];
      const float bore_value = a + (b - a) * delay_fractional;

      // The in-loop half-sample averager, 1344-1345. Its zero sits at the
      float pressure_delta = 0.5f * (bore_value + lp_state_);
      lp_state_ = bore_value;

      // Inverting bore reflection, 1347.
      pressure_delta *= kBlownReflection;
      pressure_delta -= breath;

      // The reed characteristic, 1349-1351: a clipped line, multiplying the
      float reed = kBlownReedSlope * pressure_delta + reed_offset;
      CONSTRAIN(reed, -kBlownClip, kBlownClip);
      float bore_input = pressure_delta * reed + breath;
      CONSTRAIN(bore_input, -kBlownClip, kBlownClip);

      // A second pickup further down the bore, read before the write so it
      float pickup_value = 0.0f;
      if (stereo) {
        const uint32_t pickup_read =
            delay_pointer_ + 2 * kBlownBoreLength - pickup_integral;
        const float pa = bore_[pickup_read & (kBlownBoreLength - 1)];
        const float pb = bore_[(pickup_read - 1) & (kBlownBoreLength - 1)];
        pickup_value = pa + (pb - pa) * pickup_fractional;
      }

      bore_[delay_pointer_ & (kBlownBoreLength - 1)] = bore_input;
      ++delay_pointer_;

      // The body filter, 1355-1356. Output only -- it is never written back
      body_state_ += body_coefficient * (bore_input - body_state_);

      float aux_sample;
      if (stereo) {
        body_state_aux_ += body_coefficient * (pickup_value - body_state_aux_);
        aux_sample = body_state_aux_;
      } else {
        aux_sample = bore_input * kBlownAuxTrim;
      }

      history_[history_write_ & 31] = body_state_;
      history_aux_[history_write_ & 31] = aux_sample;
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

    // The static breath pressure leaves the bore parked off zero -- the loop's
    ONE_POLE(out_dc_input_, decimated, kBlownDcCoefficient);
    ONE_POLE(aux_dc_input_, decimated_aux, kBlownDcCoefficient);
    out_dc_output_ = decimated - out_dc_input_;
    aux_dc_output_ = decimated_aux - aux_dc_input_;

    *out++ = out_dc_output_;
    *aux++ = aux_dc_output_;
    ++sample_index;
  }
}

}  // namespace plaits

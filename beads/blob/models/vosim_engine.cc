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
//
// Vosim: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/vosim_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float MorphBreakpoint(float morph) {
  const float amount = morph * 2.0f;
  return morph < 0.5f
      ? kVosimMinBreakpoint +
          (kVosimStockBreakpoint - kVosimMinBreakpoint) * amount
      : kVosimStockBreakpoint +
          (kVosimMaxBreakpoint - kVosimStockBreakpoint) * (amount - 1.0f);
}

const uint16_t kBell[257] = {
       0,    670,   2655,   5873,  10191,  15434,  21387,  27805,
   34427,  40980,  47198,  52824,  57630,  61417,  64032,  65366,
   65534,  65528,  65517,  65500,  65477,  65449,  65415,  65376,
   65331,  65280,  65224,  65162,  65095,  65022,  64944,  64860,
   64770,  64675,  64574,  64468,  64357,  64240,  64118,  63990,
   63857,  63718,  63575,  63426,  63271,  63112,  62947,  62777,
   62602,  62421,  62236,  62046,  61850,  61650,  61444,  61234,
   61018,  60798,  60573,  60343,  60109,  59870,  59626,  59377,
   59124,  58866,  58604,  58338,  58067,  57791,  57512,  57228,
   56940,  56648,  56351,  56051,  55746,  55438,  55126,  54810,
   54490,  54166,  53839,  53508,  53173,  52835,  52494,  52149,
   51801,  51449,  51094,  50736,  50375,  50011,  49645,  49275,
   48902,  48526,  48148,  47767,  47384,  46998,  46610,  46219,
   45826,  45431,  45033,  44633,  44232,  43828,  43423,  43015,
   42606,  42195,  41783,  41369,  40953,  40537,  40118,  39699,
   39278,  38856,  38433,  38010,  37585,  37159,  36733,  36306,
   35879,  35450,  35022,  34593,  34163,  33734,  33304,  32874,
   32445,  32015,  31585,  31156,  30727,  30298,  29869,  29442,
   29014,  28588,  28162,  27737,  27312,  26889,  26467,  26045,
   25625,  25206,  24789,  24373,  23958,  23545,  23133,  22723,
   22315,  21908,  21504,  21101,  20700,  20302,  19905,  19511,
   19119,  18730,  18343,  17958,  17576,  17196,  16819,  16445,
   16074,  15706,  15340,  14978,  14618,  14262,  13909,  13559,
   13212,  12869,  12529,  12193,  11860,  11531,  11206,  10884,
   10566,  10252,   9941,   9635,   9332,   9034,   8740,   8450,
    8164,   7882,   7604,   7331,   7062,   6798,   6538,   6283,
    6032,   5786,   5544,   5307,   5075,   4848,   4625,   4407,
    4195,   3987,   3784,   3586,   3393,   3205,   3022,   2844,
    2671,   2504,   2342,   2185,   2033,   1887,   1746,   1610,
    1479,   1354,   1235,   1121,   1012,    909,    811,    719,
     632,    550,    475,    405,    340,    281,    228,    180,
     138,    101,     70,     45,     25,     11,      2,      0,
       0
};

inline float Bell(float phase) {
  float index = phase * 256.0f;
  CONSTRAIN(index, 0.0f, 255.999f);
  const int integral = static_cast<int>(index);
  const float fractional = index - static_cast<float>(integral);
  const float a = static_cast<float>(kBell[integral]);
  const float b = static_cast<float>(kBell[integral + 1]);
  return (a + (b - a) * fractional) * (1.0f / 65536.0f);
}

inline float FormantNote(float knob) {
  int parameter = static_cast<int>(knob * 32767.0f);
  CONSTRAIN(parameter, 0, 32767);
  return static_cast<float>(parameter >> 1) * (1.0f / 128.0f);
}

inline float WarpPhase(float phase, float breakpoint,
                       float attack_slope, float decay_slope) {
  return phase < breakpoint
      ? phase * attack_slope
      : kVosimStockBreakpoint + (phase - breakpoint) * decay_slope;
}

}  // namespace

inline float Vosim::Decimate(const float* history) const {
  const int p = history_position_;
  return kVosimHalfbandCentre * history[(p - 7) & 15]
      + kVosimHalfband1 * (history[(p - 6) & 15] + history[(p - 8) & 15])
      + kVosimHalfband3 * (history[(p - 4) & 15] + history[(p - 10) & 15])
      + kVosimHalfband5 * (history[(p - 2) & 15] + history[(p - 12) & 15])
      + kVosimHalfband7 * (history[p] + history[(p - 14) & 15]);
}

void Vosim::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Vosim::Reset() {
  phase_ = 0.0f;
  formant_phase_[0] = 0.75f;
  formant_phase_[1] = 0.75f;
  for (int i = 0; i < 16; ++i) {
    history_[i] = 0.0f;
    history_aux_[i] = 0.0f;
  }
  history_position_ = 0;
  dc_input_ = 0.0f;
  dc_input_aux_ = 0.0f;
  dc_seeded_ = false;
  seed_step_ = 0;
  seed_phase_[0] = 0.75f;
  seed_phase_[1] = 0.75f;
  sum_window_ = 0.0f;
  sum_formant_[0] = 0.0f;
  sum_formant_[1] = 0.0f;
}

void Vosim::Prepare(const EngineParameters& parameters) {
  if (seed_step_ == 0) {
    seed_parameters_ = parameters;
  }
  const float increment = NoteToFrequency(seed_parameters_.note) * 0.5f;
  const float formant_increment_1 =
      NoteToFrequency(FormantNote(seed_parameters_.timbre)) * 0.5f;
  const float formant_increment_2 =
      NoteToFrequency(FormantNote(seed_parameters_.harmonics)) * 0.5f;

  const float breakpoint = MorphBreakpoint(seed_parameters_.morph);
  const float attack_slope = kVosimStockBreakpoint / breakpoint;
  const float decay_slope =
      (1.0f - kVosimStockBreakpoint) / (1.0f - breakpoint);

  const float share = kVosimStockShare;
  float share_main = share;
  float share_aux = 1.0f - share;

  const float pedestal = kVosimPedestal * (1.0f + kVosimSineOffset);
  const float a1_main = kVosimSineAmplitude * kVosimPedestal * share_main;
  const float a2_main = kVosimSineAmplitude * kVosimPedestal - a1_main;
  const float a1_aux = kVosimSineAmplitude * kVosimPedestal * share_aux;
  const float a2_aux = kVosimSineAmplitude * kVosimPedestal - a1_aux;

  // Continue the selection integral without advancing the oscillator.
  const float step = 1.0f / static_cast<float>(kVosimSeedSteps);
  const float ratio_1 = formant_increment_1 / increment;
  const float ratio_2 = formant_increment_2 / increment;
  const bool integrate_1 = ratio_1 < kVosimSeedMaxRatio;
  const bool integrate_2 = ratio_2 < kVosimSeedMaxRatio;
  const float delta_1 = integrate_1 ? ratio_1 * step : 0.0f;
  const float delta_2 = integrate_2 ? ratio_2 * step : 0.0f;
  const int end = seed_step_ + kVosimSeedStepsPerBlock;
  for (int k = seed_step_; k < end; ++k) {
    const float carrier = static_cast<float>(k) * step;
    const float window = Bell(
        WarpPhase(carrier, breakpoint, attack_slope, decay_slope));
    sum_window_ += window;
    if (integrate_1) {
      sum_formant_[0] += window * SineNoWrap(seed_phase_[0]);
      seed_phase_[0] += delta_1;
      if (seed_phase_[0] >= 1.0f) {
        seed_phase_[0] -= 1.0f;
      }
    }
    if (integrate_2) {
      sum_formant_[1] += window * SineNoWrap(seed_phase_[1]);
      seed_phase_[1] += delta_2;
      if (seed_phase_[1] >= 1.0f) {
        seed_phase_[1] -= 1.0f;
      }
    }
  }
  seed_step_ = end;
  if (seed_step_ != kVosimSeedSteps) {
    return;
  }
  dc_input_ = (pedestal * sum_window_
      + a1_main * sum_formant_[0] + a2_main * sum_formant_[1]) * step
      - kVosimPedestal;
  dc_input_aux_ = (pedestal * sum_window_
      + a1_aux * sum_formant_[0] + a2_aux * sum_formant_[1]) * step
      - kVosimPedestal;
  dc_seeded_ = true;
}

void Vosim::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (!dc_seeded_) {
    Prepare(parameters);
    fill(&out[0], &out[size], 0.0f);
    fill(&aux[0], &aux[size], 0.0f);
    return;
  }

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    formant_phase_[0] = 0.75f;
    formant_phase_[1] = 0.75f;
  }

  const float increment = NoteToFrequency(parameters.note) * 0.5f;
  const float formant_increment_1 =
      NoteToFrequency(FormantNote(parameters.timbre)) * 0.5f;
  const float formant_increment_2 =
      NoteToFrequency(FormantNote(parameters.harmonics)) * 0.5f;

  const float breakpoint = MorphBreakpoint(parameters.morph);
  const float attack_slope = kVosimStockBreakpoint / breakpoint;
  const float decay_slope =
      (1.0f - kVosimStockBreakpoint) / (1.0f - breakpoint);

  const float share = kVosimStockShare;
  float share_main = share;
  float share_aux = 1.0f - share;

  const float pedestal = kVosimPedestal * (1.0f + kVosimSineOffset);
  const float a1_main = kVosimSineAmplitude * kVosimPedestal * share_main;
  const float a2_main = kVosimSineAmplitude * kVosimPedestal - a1_main;
  const float a1_aux = kVosimSineAmplitude * kVosimPedestal * share_aux;
  const float a2_aux = kVosimSineAmplitude * kVosimPedestal - a1_aux;

  for (size_t i = 0; i < size; ++i) {
    float carrier_increment = increment;
    CONSTRAIN(carrier_increment, -0.25f, 0.249999f);
    for (int s = 0; s < 2; ++s) {
      bool carrier_wrapped = false;
      phase_ += carrier_increment;
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
        carrier_wrapped = true;
      } else if (phase_ < 0.0f) {
        phase_ += 1.0f;
        carrier_wrapped = true;
      }
      formant_phase_[0] += formant_increment_1;
      if (formant_phase_[0] >= 1.0f) {
        formant_phase_[0] -= 1.0f;
      }
      formant_phase_[1] += formant_increment_2;
      if (formant_phase_[1] >= 1.0f) {
        formant_phase_[1] -= 1.0f;
      }

      const float f1 = SineNoWrap(formant_phase_[0]);
      const float f2 = SineNoWrap(formant_phase_[1]);
      const float window = Bell(
          WarpPhase(phase_, breakpoint, attack_slope, decay_slope));

      float sample = (pedestal + a1_main * f1 + a2_main * f2) * window;
      float sample_aux = (pedestal + a1_aux * f1 + a2_aux * f2) * window;

      if (carrier_wrapped) {
        formant_phase_[0] = 0.75f;
        formant_phase_[1] = 0.75f;
        sample = 0.0f;
        sample_aux = 0.0f;
      }

      history_position_ = (history_position_ + 1) & 15;
      history_[history_position_] = sample - kVosimPedestal;
      history_aux_[history_position_] = sample_aux - kVosimPedestal;
    }

    const float raw = Decimate(history_);
    const float raw_aux = Decimate(history_aux_);
    ONE_POLE(dc_input_, raw, 0.001f);
    ONE_POLE(dc_input_aux_, raw_aux, 0.001f);
    out[i] = raw - dc_input_;
    aux[i] = raw_aux - dc_input_aux_;
  }
}

}  // namespace plaits

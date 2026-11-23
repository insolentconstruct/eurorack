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
// NoiseBank: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/noise_bank_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const int16_t kOverdrive[257] = {
  -32766, -32728, -32689, -32648, -32607, -32564, -32519, -32474,
  -32427, -32378, -32328, -32277, -32224, -32170, -32113, -32056,
  -31996, -31935, -31872, -31807, -31740, -31671, -31600, -31527,
  -31451, -31374, -31294, -31212, -31128, -31041, -30951, -30859,
  -30765, -30667, -30567, -30464, -30358, -30250, -30138, -30022,
  -29904, -29782, -29657, -29529, -29397, -29261, -29122, -28979,
  -28832, -28681, -28526, -28367, -28204, -28036, -27864, -27688,
  -27507, -27321, -27131, -26936, -26736, -26531, -26321, -26106,
  -25886, -25660, -25429, -25192, -24950, -24702, -24449, -24190,
  -23925, -23654, -23377, -23094, -22805, -22510, -22209, -21902,
  -21588, -21268, -20942, -20609, -20270, -19924, -19573, -19215,
  -18850, -18479, -18102, -17718, -17328, -16932, -16530, -16121,
  -15707, -15286, -14859, -14427, -13989, -13545, -13095, -12640,
  -12180, -11715, -11244, -10769, -10289,  -9804,  -9315,  -8822,
   -8324,  -7823,  -7319,  -6810,  -6299,  -5785,  -5268,  -4748,
   -4226,  -3703,  -3177,  -2650,  -2121,  -1592,  -1062,   -531,
       0,    531,   1062,   1592,   2122,   2650,   3177,   3703,
    4227,   4749,   5268,   5785,   6299,   6811,   7319,   7824,
    8325,   8822,   9315,   9804,  10289,  10769,  11244,  11715,
   12180,  12641,  13095,  13545,  13989,  14427,  14860,  15286,
   15707,  16122,  16530,  16933,  17329,  17719,  18102,  18479,
   18850,  19215,  19573,  19925,  20270,  20609,  20942,  21268,
   21588,  21902,  22209,  22510,  22806,  23094,  23377,  23654,
   23925,  24190,  24449,  24703,  24950,  25192,  25429,  25660,
   25886,  26106,  26321,  26531,  26736,  26936,  27131,  27322,
   27507,  27688,  27865,  28037,  28204,  28367,  28526,  28681,
   28832,  28979,  29122,  29262,  29397,  29529,  29658,  29783,
   29904,  30023,  30138,  30250,  30359,  30465,  30568,  30668,
   30765,  30860,  30952,  31041,  31128,  31212,  31294,  31374,
   31452,  31527,  31600,  31671,  31740,  31807,  31872,  31935,
   31996,  32056,  32114,  32170,  32224,  32277,  32329,  32379,
   32427,  32474,  32520,  32564,  32607,  32648,  32689,  32728,
   32728,
};

const uint16_t kResonatorCoefficient[129] = {
   65535,  65535,  65535,  65535,  65535,  65535,  65535,  65535,
   65535,  65535,  65535,  65535,  65535,  65535,  65535,  65535,
   65535,  65535,  65535,  65535,  65535,  65535,  65535,  65535,
   65535,  65535,  65535,  65535,  65535,  65534,  65534,  65534,
   65534,  65534,  65534,  65533,  65533,  65533,  65532,  65532,
   65532,  65531,  65531,  65530,  65529,  65529,  65528,  65527,
   65526,  65525,  65523,  65522,  65520,  65518,  65516,  65514,
   65511,  65508,  65505,  65501,  65497,  65492,  65487,  65481,
   65475,  65467,  65459,  65449,  65439,  65427,  65414,  65399,
   65382,  65363,  65342,  65318,  65292,  65262,  65228,  65191,
   65149,  65101,  65048,  64988,  64922,  64847,  64762,  64668,
   64562,  64443,  64310,  64160,  63992,  63804,  63593,  63356,
   63091,  62794,  62461,  62088,  61670,  61202,  60677,  60091,
   59435,  58701,  57881,  56964,  55941,  54799,  53526,  52107,
   50528,  48773,  46824,  44662,  42268,  39623,  36704,  33492,
   29966,  26107,  21898,  17325,  12380,   7061,   1374,      0,
       0,
};

const uint16_t kResonatorScale[129] = {
     1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
     1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
     1,    2,    2,    2,    2,    2,    3,    3,    3,    4,
     4,    4,    5,    5,    6,    6,    7,    8,    8,    9,
    10,   11,   12,   13,   14,   16,   17,   19,   20,   22,
    24,   27,   29,   32,   35,   38,   41,   45,   49,   54,
    58,   64,   70,   76,   83,   90,   98,  107,  117,  128,
   139,  152,  166,  181,  197,  215,  234,  255,  256,  256,
   256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
   256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
   256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
   256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
   256,  256,  256,  256,  256,  256,  256,  256,  256,
};

inline float Shaper(float x) {
  float index = (x + 1.0f) * 128.0f;
  CONSTRAIN(index, 0.0f, 255.999f);
  const int integral = static_cast<int>(index);
  const float fractional = index - static_cast<float>(integral);
  const float a = static_cast<float>(kOverdrive[integral]);
  const float b = static_cast<float>(kOverdrive[integral + 1]);
  return (a + (b - a) * fractional) * (1.0f / 32768.0f);
}

inline float ReadNoteTable(const uint16_t* table, float note) {
  CONSTRAIN(note, 0.0f, 127.999f);
  const int integral = static_cast<int>(note);
  const float fractional = note - static_cast<float>(integral);
  const float a = static_cast<float>(table[integral]);
  const float b = static_cast<float>(table[integral + 1]);
  return a + (b - a) * fractional;
}

inline uint32_t ResonatorCoefficient(float note) {
  return static_cast<uint32_t>(ReadNoteTable(kResonatorCoefficient, note));
}

inline float SvfCutoffAt(int note) {
  float f = NoteToFrequency(static_cast<float>(note)) * 0.5f;
  if (f > kNoiseBankMaxCutoff) {
    f = kNoiseBankMaxCutoff;
  }
  return 2.0f * sinf(3.14159265358979f * f);
}

inline float SvfDampAt(int index) {
  const float resonance = static_cast<float>(index) /
      kNoiseBankResonanceDivisor;
  return 2.0f * (1.0f - Sqrt(Sqrt(resonance)));
}

inline float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

inline float Decimate(const float* buffer, uint32_t write) {
#define NB_TAP(j) buffer[(write - 1u - (j)) & kNoiseBankDecimatorMask]
  return 0.49863366f * NB_TAP(9)
      + 0.30863382f * (NB_TAP(8) + NB_TAP(10))
      - 0.08147628f * (NB_TAP(6) + NB_TAP(12))
      + 0.02921222f * (NB_TAP(4) + NB_TAP(14))
      - 0.00850828f * (NB_TAP(2) + NB_TAP(16))
      + 0.00282169f * (NB_TAP(0) + NB_TAP(18));
#undef NB_TAP
}

inline int32_t Parameter(float macro) {
  int32_t value = static_cast<int32_t>(macro * 32767.0f);
  CONSTRAIN(value, 0, 32767);
  return value;
}

inline int32_t NoiseSample() {
  return static_cast<int32_t>(
      static_cast<int16_t>(Random::GetWord() >> 16)) >> 1;
}

}  // namespace

void NoiseBank::Init(BufferAllocator* allocator) {
  (void) allocator;
  model_quantizer_.Init(NOISE_BANK_MODEL_LAST, 0.05f, false);
  Reset();
}

void NoiseBank::Reset() {
  lp_ = 0.0f;
  bp_ = 0.0f;
  y11_ = 0;
  y12_ = 0;
  y21_ = 0;
  y22_ = 0;
  twin_main_ = 0.0f;
  twin_aux_ = 0.0f;
  clock_phase_ = 0;
  cycle_phase_ = 0;
  rng_state_ = 0;
  rng_state_aux_ = 0x9e3779b9;
  seed_ = 0;
  seed_aux_ = 0x9e3779b9;
  clocked_sample_ = 0;
  clocked_sample_aux_ = 0;
  drive_ = 1.0f;
  for (size_t i = 0; i < kNoiseBankDecimatorSize; ++i) {
    decimator_main_[i] = 0.0f;
    decimator_aux_[i] = 0.0f;
  }
  decimator_write_ = 0;
}

void NoiseBank::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const int model = model_quantizer_.Process(parameters.harmonics);

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    seed_ = Random::GetWord();
    seed_aux_ = Random::GetWord();
    cycle_phase_ = 0;
  }

  float note = parameters.note;
  CONSTRAIN(note, 0.0f, kNoiseBankMaxNote);

  const bool clocked = model == NOISE_BANK_MODEL_CLOCKED;
  const float target_drive = clocked ? 0.0f : 1.0f;
  const float shaper_mix = clocked ? min(1.0f, target_drive) : 1.0f;

  if (model == NOISE_BANK_MODEL_FILTERED) {
    const int note_integral = static_cast<int>(note);
    const float base_f = Lerp(SvfCutoffAt(note_integral),
                              SvfCutoffAt(note_integral + 1),
                              note - static_cast<float>(note_integral));

    float damp_index = static_cast<float>(Parameter(parameters.timbre)) *
        kNoiseBankDampIndexScale;
    CONSTRAIN(damp_index, 0.0f, 255.999f);
    const int damp_integral = static_cast<int>(damp_index);
    const float damp_fractional =
        damp_index - static_cast<float>(damp_integral);
    const float damp_a = SvfDampAt(damp_integral);
    const float damp_b = SvfDampAt(damp_integral + 1);
    const float damp = Lerp(damp_a, damp_b, damp_fractional);
    const float scale = Lerp(Sqrt(damp_a * 0.5f), Sqrt(damp_b * 0.5f),
                             damp_fractional);

    const float p1 = static_cast<float>(Parameter(parameters.morph));
    float lp_gain;
    float bp_gain;
    float hp_gain;
    if (p1 < 16384.0f) {
      bp_gain = p1 * (1.0f / 16384.0f);
      lp_gain = 1.0f - bp_gain;
      hp_gain = 0.0f;
    } else {
      bp_gain = (32767.0f - p1) * (1.0f / 16384.0f);
      hp_gain = (p1 - 16384.0f) * (1.0f / 16384.0f);
      lp_gain = 0.0f;
    }

    ParameterInterpolator drive_modulation(&drive_, target_drive, size);

    for (size_t i = 0; i < size; ++i) {
      const float drive = drive_modulation.Next();
      float f = base_f;
      const float gain_correction = f > scale ? scale / f : 1.0f;
      for (int s = 0; s < 2; ++s) {
        const float in = static_cast<float>(NoiseSample()) *
            (1.0f / 32768.0f);

        const float notch = in - bp_ * damp;
        lp_ += f * bp_;
        CONSTRAIN(lp_, -1.0f, 1.0f);
        const float hp = notch - lp_;
        bp_ += f * hp;

        float result = lp_gain * lp_ + bp_gain * bp_ + hp_gain * hp;
        CONSTRAIN(result, -1.0f, 1.0f);
        float side = notch;
        CONSTRAIN(side, -1.0f, 1.0f);

        result *= gain_correction;
        side *= gain_correction;
        Push(Shaper(result * drive), Shaper(side * drive));
      }
      out[i] = Decimate(decimator_main_, decimator_write_);
      aux[i] = Decimate(decimator_aux_, decimator_write_);
    }
  } else if (model == NOISE_BANK_MODEL_TWIN_PEAKS) {
    const int32_t parameter = Parameter(parameters.timbre);

    const uint32_t q = 65240 + (static_cast<uint32_t>(parameter) >> 7);
    const int32_t q_squared = static_cast<int32_t>(q * q >> 17);
    const int32_t makeup_gain = 8191 - (parameter >> 2);

    const int32_t color = Parameter(parameters.morph);
    float note_2 = note + static_cast<float>((color - 16384) >> 1) *
        (1.0f / 128.0f);
    CONSTRAIN(note_2, 0.0f, 127.999f);

    const int32_t base_c1 = static_cast<int32_t>(
        ResonatorCoefficient(note) * q >> 16);
    const int32_t base_c2 = static_cast<int32_t>(
        ResonatorCoefficient(note_2) * q >> 16);

    const int32_t base_s1 = static_cast<int32_t>(
        ReadNoteTable(kResonatorScale, note));
    const int32_t base_s2 = static_cast<int32_t>(
        ReadNoteTable(kResonatorScale, note_2));

    ParameterInterpolator drive_modulation(&drive_, target_drive, size);

    for (size_t i = 0; i < size; ++i) {
      const float drive = drive_modulation.Next();
      int32_t c1 = base_c1;
      int32_t c2 = base_c2;
      int32_t s1 = base_s1;
      int32_t s2 = base_s2;

      const int32_t n = NoiseSample();
      int32_t y10 = n > 0 ? (n * s1) >> 16 : -(((-n) * s1) >> 16);
      int32_t y20 = n > 0 ? (n * s2) >> 16 : -(((-n) * s2) >> 16);

      y10 += y11_ * c1 >> 15;
      y10 -= y12_ * q_squared >> 15;
      CONSTRAIN(y10, -32767, 32767);
      y12_ = y11_;
      y11_ = y10;

      y20 += y21_ * c2 >> 15;
      y20 -= y22_ * q_squared >> 15;
      CONSTRAIN(y20, -32767, 32767);
      y22_ = y21_;
      y21_ = y20;

      int32_t sum = y10 + y20;
      sum += sum * makeup_gain >> 13;
      CONSTRAIN(sum, -32767, 32767);
      int32_t difference = y10 - y20;
      difference += difference * makeup_gain >> 13;
      CONSTRAIN(difference, -32767, 32767);

      twin_main_ = Shaper(static_cast<float>(sum) * (1.0f / 32768.0f) * drive);
      twin_aux_ = Shaper(
          static_cast<float>(difference) * (1.0f / 32768.0f) * drive);

      Push(twin_main_, twin_aux_);
      Push(twin_main_, twin_aux_);

      out[i] = Decimate(decimator_main_, decimator_write_);
      aux[i] = Decimate(decimator_aux_, decimator_write_);
    }
  } else {
    float increment = NoteToFrequency(note) * 0.5f;
    CONSTRAIN(increment, 0.0f, 0.4999f);
    uint32_t base_phase_increment = static_cast<uint32_t>(
        increment * 4294967296.0f);
    for (int i = 0; i < kNoiseBankClockShifts; ++i) {
      if (base_phase_increment < (1UL << 31)) {
        base_phase_increment <<= 1;
      }
    }

    const float cycle_note = static_cast<float>(
        Parameter(parameters.timbre) - 16384) * (1.0f / 128.0f);
    float cycle = NoteToFrequency(cycle_note);
    CONSTRAIN(cycle, 0.0f, 0.4999f);
    const uint32_t cycle_phase_increment = static_cast<uint32_t>(
        cycle * 4294967296.0f);

    uint32_t num_steps =
        1 + (static_cast<uint32_t>(Parameter(parameters.morph)) >> 10);
    if (num_steps == 1) {
      num_steps = 2;
    }
    const uint32_t quantizer_divider = 65536 / num_steps;

    ParameterInterpolator drive_modulation(&drive_, target_drive, size);

    for (size_t i = 0; i < size; ++i) {
      const float drive = drive_modulation.Next();
      uint32_t phase_increment = base_phase_increment;
      for (int s = 0; s < 2; ++s) {
        clock_phase_ += phase_increment;
        if (clock_phase_ < phase_increment) {
          rng_state_ = rng_state_ * 1664525u + 1013904223u;
          rng_state_aux_ = rng_state_aux_ * 1664525u + 1013904223u;
          cycle_phase_ += cycle_phase_increment;
          if (cycle_phase_ < cycle_phase_increment) {
            rng_state_ = seed_;
            rng_state_aux_ = seed_aux_;
            cycle_phase_ = cycle_phase_increment;
          }
          uint16_t sample = static_cast<uint16_t>(rng_state_);
          sample -= sample % quantizer_divider;
          sample += quantizer_divider >> 1;
          clocked_sample_ = static_cast<int16_t>(sample);

          uint16_t sample_aux = static_cast<uint16_t>(rng_state_aux_);
          sample_aux -= sample_aux % quantizer_divider;
          sample_aux += quantizer_divider >> 1;
          clocked_sample_aux_ = static_cast<int16_t>(sample_aux);

          clock_phase_ = phase_increment;
        }
        const float raw = static_cast<float>(clocked_sample_) *
            (1.0f / 32768.0f);
        const float raw_aux = static_cast<float>(clocked_sample_aux_) *
            (1.0f / 32768.0f);
        Push(raw + (Shaper(raw * drive) - raw) * shaper_mix,
             raw_aux + (Shaper(raw_aux * drive) - raw_aux) * shaper_mix);
      }
      out[i] = Decimate(decimator_main_, decimator_write_);
      aux[i] = Decimate(decimator_aux_, decimator_write_);
    }
  }
}

}  // namespace plaits

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
// Vowel: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/vowel_engine.h"


#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace stmlib;

namespace {

inline float ApplyMacro(float stock, float minimum, float maximum, float macro) {
  const float amount = macro * 2.0f;
  return macro < 0.5f
      ? minimum + (stock - minimum) * amount
      : stock + (maximum - stock) * (amount - 1.0f);
}

}  // namespace


namespace {

// Braids' wav_formant_sine, verbatim from braids/resources.cc, reproduced at

const int8_t kFormantSine[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   2,   2,   3,   3,   4,   5,   6,   7,   8,  10,  12,  14,  17,  20,  24,
    0,   3,   4,   5,   6,   7,   9,  10,  12,  15,  18,  21,  26,  31,  37,  45,
    0,   4,   5,   6,   8,   9,  11,  13,  16,  19,  23,  28,  34,  40,  49,  58,
    0,   5,   6,   7,   8,  10,  12,  15,  17,  21,  25,  30,  36,  44,  53,  63,
    0,   4,   5,   6,   8,   9,  11,  13,  16,  19,  23,  28,  34,  40,  49,  58,
    0,   3,   4,   5,   6,   7,   9,  10,  12,  15,  18,  21,  26,  31,  37,  45,
    0,   2,   2,   3,   3,   4,   5,   6,   7,   8,  10,  12,  14,  17,  20,  24,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,  -2,  -2,  -3,  -3,  -4,  -5,  -6,  -7,  -8, -10, -12, -14, -17, -20, -24,
    0,  -3,  -4,  -5,  -6,  -7,  -9, -10, -12, -15, -18, -21, -26, -31, -37, -45,
    0,  -4,  -5,  -6,  -8,  -9, -11, -13, -16, -19, -23, -28, -34, -40, -49, -58,
    0,  -5,  -6,  -7,  -8, -10, -12, -15, -17, -21, -25, -30, -36, -44, -53, -63,
    0,  -4,  -5,  -6,  -8,  -9, -11, -13, -16, -19, -23, -28, -34, -40, -49, -58,
    0,  -3,  -4,  -5,  -6,  -7,  -9, -10, -12, -15, -18, -21, -26, -31, -37, -45,
    0,  -2,  -2,  -3,  -3,  -4,  -5,  -6,  -7,  -8, -10, -12, -14, -17, -20, -24
};

// wav_formant_square, factorised. Braids stores a full 256-entry table, but
const int8_t kFormantSquareGain[16] = {
   0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  6,  8,  9, 11, 13, 16
};

// ws_moderate_overdrive, verbatim from braids/resources.cc, reproduced at
const int16_t kModerateOverdrive[257] = {
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
   32728
};

// The phoneme tables, lifted from digital_oscillator.cc:445-466 (vowels_data
const uint8_t kVowelFrequency[27] = {
   27,  40,  89,
   18,  51,  62,
   15,  69,  93,
   10,  84, 110,
   23,  44,  87,
   13,  29,  80,
    6,  46,  81,
    9,  51,  95,
    6,  73,  99
};

const uint8_t kVowelAmplitude[27] = {
  15, 13,  1,
  13, 12,  6,
  14, 12,  7,
  13, 10,  8,
  15, 12,  1,
  13,  8,  0,
  12,  3,  0,
  15,  3,  0,
   7,  3, 14
};

const uint8_t kConsonantFrequency[24] = {
    6,  54, 121,
   18,  50,  51,
   11,  24,  70,
   15,  69,  74,
   16,  37, 111,
   18,  51,  62,
    6,  26,  81,
    6,  73,  99
};

// Halfwords keep adjacent amplitudes from resembling absolute addresses.
const uint16_t kConsonantAmplitude[24] = {
   9,  9,  0,
  12, 10,  5,
  13,  8,  0,
  14, 12,  7,
  14,  8,  1,
  14, 12,  6,
   5,  5,  5,
   7, 10, 14
};

// The 2:1 decimator. 31-tap windowed-sinc halfband, fc = 0.25 of the 96 kHz
const float kHalfband[9] = {
  0.5000007671f,
  0.3103159181f,
  -0.0842284807f,
  0.0331408846f,
  -0.0121862031f,
  0.0036432918f,
  -0.0007726768f,
  0.0000881559f,
  -0.0000012732f
};

// Each 32-sample history is mirrored into a 64-entry buffer. With `write` one
inline float Decimate(const float* buffer, int write) {
  const float* centre =
      buffer + (write & kVowelFirMask) + kVowelFirSize / 2;
  float sum = kHalfband[0] * centre[0];
  for (int k = 1; k < 9; ++k) {
    const int offset = 2 * k - 1;
    sum += kHalfband[k] * (centre[offset] + centre[-offset]);
  }
  return sum;
}

// stmlib::Interpolate88 on the shaper table: the int16 sum becomes a uint16
inline float ReadOverdrive(int sample) {
  const int index = sample + 32768;
  const int integral = index >> 8;
  const int fractional = index & 0xff;
  const int a = kModerateOverdrive[integral];
  const int b = kModerateOverdrive[integral + 1];
  return static_cast<float>(a + (((b - a) * fractional) >> 8)) *
      (1.0f / 32768.0f);
}

// digital_oscillator.cc:511/515 for the two sine formants and :519 for the
inline int ReadFormant(uint32_t phase, int amplitude) {
  return kFormantSine[((phase >> 24) & 0xf0) | amplitude];
}

inline int ReadSquareFormant(uint32_t phase, int amplitude) {
  const int gain = kFormantSquareGain[amplitude];
  return (phase & 0x80000000u) ? -gain : gain;
}

// Braids' increment is a 29-bit integer, which does not survive a float round
inline uint32_t ScaleIncrement(uint32_t increment, float scale) {
  uint32_t result = increment;
  if (scale != 1.0f) {
    // The largest table-derived increment is 121 * 0x1000 * 711 =
    result = static_cast<uint32_t>(
        static_cast<float>(increment) * scale);
  }
  if (result > kVowelMaxFormantIncrement) {
    result = kVowelMaxFormantIncrement;
  }
  return result;
}

}  // namespace

// stmlib::Random::GetSample(): an LCG whose state starts at 0x21
int Vowel::NextRandomSample() {
  rng_state_ = rng_state_ * 1664525u + 1013904223u;
  return static_cast<int16_t>(rng_state_ >> 16);
}

void Vowel::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

// DigitalOscillator::Init() (digital_oscillator.h:246) zeroes the state, zeroes
void Vowel::Reset() {
  phase_ = 0;
  phase_increment_ = 0;
  for (int i = 0; i < kVowelNumFormants; ++i) {
    formant_phase_[i] = 0;
    formant_phase_r_[i] = 0;
    formant_increment_[i] = 0;
    formant_amplitude_[i] = 0;
  }
  noise_ = 0;
  consonant_samples_ = 0;
  strike_pending_ = true;
  rng_state_ = 0x21;
  for (int i = 0; i < kVowelFirBufferSize; ++i) {
    fir_a_[i] = 0.0f;
    fir_b_[i] = 0.0f;
  }
  fir_write_ = 0;
  dc_input_ = 0.0f;
  dc_output_ = 0.0f;
  dc_input_aux_ = 0.0f;
  dc_output_aux_ = 0.0f;
}

void Vowel::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  // Braids' Strike() only raises the flag (digital_oscillator.h:284) -- it does
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    strike_pending_ = true;
  }

  const bool stereo = false;

  float note = parameters.note;
  if (note > kVowelHighestNote) {
    note = kVowelHighestNote;
  }
  // The inner loop runs at 96 kHz, so the pitch increment is half a 48 kHz one.
  phase_increment_ = static_cast<uint32_t>(
      NoteToFrequency(note) * 0.5f * 4294967296.0f);

  // Braids' two knobs, reconstructed in their integer form so that the frame
  int p_vowel = static_cast<int>(parameters.morph * 32767.0f);
  int p_shift = static_cast<int>(parameters.timbre * 32767.0f);
  CONSTRAIN(p_vowel, 0, 32767);
  CONSTRAIN(p_shift, 0, 32767);
  const int vowel_index = p_vowel >> 12;
  const int balance = p_vowel & 0x0fff;
  const int formant_shift = kVowelMinFormantShift + (p_shift >> 6);

  const float spread = ApplyMacro(
      1.0f, kVowelMinSpread, kVowelMaxSpread, parameters.harmonics);
  const float grain = 1.0f;
  // At MACRO's detent ApplyMacro returns exactly 1.0f, so this is exactly
  const float inverse_grain = 1.0f / grain;

  // Braids' per-block frame update, digital_oscillator.cc:476-502. Note the
  if (strike_pending_) {
    strike_pending_ = false;
    consonant_samples_ = kVowelConsonantSamples;
    const int index = (NextRandomSample() + 1) & 7;
    for (int i = 0; i < kVowelNumFormants; ++i) {
      formant_increment_[i] = static_cast<uint32_t>(
          kConsonantFrequency[index * kVowelNumFormants + i]) * 0x1000u *
          static_cast<uint32_t>(formant_shift);
      formant_amplitude_[i] =
          kConsonantAmplitude[index * kVowelNumFormants + i];
    }
    // Consonants 6 and 7 are the unvoiced ones: they jitter the period reset
    noise_ = index >= 6 ? 4095 : 0;
  }

  if (consonant_samples_ > 0) {
    consonant_samples_ -= kVowelOversampling * static_cast<int>(size);
    if (consonant_samples_ < 0) {
      consonant_samples_ = 0;
    }
  } else {
    for (int i = 0; i < kVowelNumFormants; ++i) {
      const int a = kVowelFrequency[vowel_index * kVowelNumFormants + i];
      const int b = kVowelFrequency[(vowel_index + 1) * kVowelNumFormants + i];
      formant_increment_[i] = static_cast<uint32_t>(
          a * (0x1000 - balance) + b * balance) *
          static_cast<uint32_t>(formant_shift);
      const int ampl_a = kVowelAmplitude[vowel_index * kVowelNumFormants + i];
      const int ampl_b =
          kVowelAmplitude[(vowel_index + 1) * kVowelNumFormants + i];
      // A TRUNCATED integer index, not a float gain: the amplitude ladder is
      formant_amplitude_[i] =
          (ampl_a * (0x1000 - balance) + ampl_b * balance) >> 12;
    }
    noise_ = 0;
  }

  uint32_t increment_l[kVowelNumFormants];
  uint32_t increment_r[kVowelNumFormants];
  for (int i = 0; i < kVowelNumFormants; ++i) {
    const float shape = i == 0 ? 1.0f : spread;
    increment_l[i] = ScaleIncrement(formant_increment_[i],
        stereo ? shape * (1.0f - kVowelStereoDetune) : shape);
    increment_r[i] = ScaleIncrement(formant_increment_[i],
        shape * (1.0f + kVowelStereoDetune));
  }

  const int amplitude_0 = formant_amplitude_[0];
  const int amplitude_1 = formant_amplitude_[1];
  const int amplitude_2 = formant_amplitude_[2];

  for (size_t i = 0; i < size; ++i) {
    uint32_t phase_increment = phase_increment_;
    for (int t = 0; t < kVowelOversampling; ++t) {
      const uint32_t before_phase = phase_;
      const bool reverse = false && (phase_increment & 0x80000000u);
      phase_ += phase_increment;

      formant_phase_[0] += reverse ? 0u - increment_l[0] : increment_l[0];
      formant_phase_[1] += reverse ? 0u - increment_l[1] : increment_l[1];
      formant_phase_[2] += reverse ? 0u - increment_l[2] : increment_l[2];
      int stack = ReadFormant(formant_phase_[0], amplitude_0) +
          ReadFormant(formant_phase_[1], amplitude_1) +
          ReadSquareFormant(formant_phase_[2], amplitude_2);

      int stack_r = 0;
      if (stereo) {
        formant_phase_r_[0] += reverse ? 0u - increment_r[0] : increment_r[0];
        formant_phase_r_[1] += reverse ? 0u - increment_r[1] : increment_r[1];
        formant_phase_r_[2] += reverse ? 0u - increment_r[2] : increment_r[2];
        stack_r = ReadFormant(formant_phase_r_[0], amplitude_0) +
            ReadFormant(formant_phase_r_[1], amplitude_1) +
            ReadSquareFormant(formant_phase_r_[2], amplitude_2);
      }

      // Braids: `sample *= 255 - (phase_ >> 24)`. Grain divides the ramp's
      int envelope = 255 - static_cast<int>(
          static_cast<float>(phase_ >> 24) * inverse_grain);
      CONSTRAIN(envelope, 0, 255);

      int sample = stack * envelope;
      int sample_r = stack_r * envelope;

      // The RNG is drawn every sample whether or not the jitter is armed, so
      const int32_t phase_noise = NextRandomSample() * noise_;
      const bool wrapped = false && false
          ? (reverse ? phase_ > before_phase : phase_ < before_phase)
          : static_cast<uint32_t>(phase_ + static_cast<uint32_t>(phase_noise)) < phase_increment_;
      if (wrapped) {
        formant_phase_[0] = 0;
        formant_phase_[1] = 0;
        formant_phase_[2] = 0;
        formant_phase_r_[0] = 0;
        formant_phase_r_[1] = 0;
        formant_phase_r_[2] = 0;
        sample = 0;
        sample_r = 0;
      }

      // int16 headroom: the largest stack over every reachable amplitude
      const int fir_index = fir_write_ & kVowelFirMask;
      const float fir_a = ReadOverdrive(sample);
      const float fir_b = stereo
          ? ReadOverdrive(sample_r)
          : static_cast<float>(sample) * (1.0f / 32768.0f);
      fir_a_[fir_index] = fir_a;
      fir_a_[fir_index + kVowelFirSize] = fir_a;
      fir_b_[fir_index] = fir_b;
      fir_b_[fir_index + kVowelFirSize] = fir_b;
      ++fir_write_;
    }

    const float decimated = Decimate(fir_a_, fir_write_);
    const float decimated_aux = Decimate(fir_b_, fir_write_);

    // The shaper is asymmetric by construction and the ramp is one-sided, so
    const float dc_out = decimated - dc_input_ + 0.9975f * dc_output_;
    dc_input_ = decimated;
    dc_output_ = dc_out;
    out[i] = dc_out;

    const float dc_aux = decimated_aux - dc_input_aux_ + 0.9975f * dc_output_aux_;
    dc_input_aux_ = decimated_aux;
    dc_output_aux_ = dc_aux;
    aux[i] = dc_aux;
  }

  fir_write_ &= kVowelFirMask;
}

}  // namespace plaits

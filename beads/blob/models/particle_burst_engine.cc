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
// ParticleBurst: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/particle_burst_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

// Braids' coefficient and scale tables retain their integer quantization.
const uint16_t kParticleResonatorCoefficient[129] = {
  65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
  65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
  65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
  65535, 65535, 65535, 65535, 65535, 65534, 65534, 65534,
  65534, 65534, 65534, 65533, 65533, 65533, 65532, 65532,
  65532, 65531, 65531, 65530, 65529, 65529, 65528, 65527,
  65526, 65525, 65523, 65522, 65520, 65518, 65516, 65514,
  65511, 65508, 65505, 65501, 65497, 65492, 65487, 65481,
  65475, 65467, 65459, 65449, 65439, 65427, 65414, 65399,
  65382, 65363, 65342, 65318, 65292, 65262, 65228, 65191,
  65149, 65101, 65048, 64988, 64922, 64847, 64762, 64668,
  64562, 64443, 64310, 64160, 63992, 63804, 63593, 63356,
  63091, 62794, 62461, 62088, 61670, 61202, 60677, 60091,
  59435, 58701, 57881, 56964, 55941, 54799, 53526, 52107,
  50528, 48773, 46824, 44662, 42268, 39623, 36704, 33492,
  29966, 26107, 21898, 17325, 12380,  7061,  1374,     0,
      0,
};

const uint16_t kParticleResonatorScale[129] = {
      1,     1,     1,     1,     1,     1,     1,     1,
      1,     1,     1,     1,     1,     1,     1,     1,
      1,     1,     1,     1,     1,     2,     2,     2,
      2,     2,     3,     3,     3,     4,     4,     4,
      5,     5,     6,     6,     7,     8,     8,     9,
     10,    11,    12,    13,    14,    16,    17,    19,
     20,    22,    24,    27,    29,    32,    35,    38,
     41,    45,    49,    54,    58,    64,    70,    76,
     83,    90,    98,   107,   117,   128,   139,   152,
    166,   181,   197,   215,   234,   255,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,
    256,
};

// Braids truncates its fixed resonance factor to this integer.
const int32_t kParticleResonanceFactorI16 = 32636;

// Braids indexes its uint16 tables with a 1/128-semitone pitch.
inline uint32_t ParticleInterpolate824(const uint16_t* table, int32_t p) {
  const uint32_t phase = static_cast<uint32_t>(p) << 17;
  const uint32_t index = phase >> 24;
  const uint32_t a = table[index];
  const uint32_t b = table[index + 1];
  const uint32_t fractional = (phase >> 8) & 0xffffu;
  return (a + (((b - a) * fractional) >> 16)) & 0xffffu;
}

// Apply Braids' truncating resonance multiply after the table read.
inline float ResonatorCoefficient(int32_t p) {
  const int32_t raw = static_cast<int32_t>(
      ParticleInterpolate824(kParticleResonatorCoefficient, p));
  return static_cast<float>((raw * kParticleResonanceFactorI16) >> 15) /
      32768.0f;
}

inline float ResonatorScale(int32_t p) {
  return static_cast<float>(
      ParticleInterpolate824(kParticleResonatorScale, p));
}

}  // namespace

void ParticleBurst::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void ParticleBurst::Reset() {
  for (int channel = 0; channel < 2; ++channel) {
    amplitude_[channel] = 0.0f;
    y1_state_[channel][0] = y1_state_[channel][1] = 0.0f;
    y2_state_[channel][0] = y2_state_[channel][1] = 0.0f;
    y3_state_[channel][0] = y3_state_[channel][1] = 0.0f;
    c1_[channel] = c2_[channel] = c3_[channel] = 0.0f;
    s1_[channel] = s2_[channel] = s3_[channel] = 0.0f;
  }
}

void ParticleBurst::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const bool stereo = false;

  // Timbre sets the trigger probability per sample.
  const uint32_t density = static_cast<uint32_t>(
      kParticleDensityFloor + parameters.timbre * kParticleDensityRange);

  // Harmonics sets jitter in Braids' signed parameter range.
  const float harmonics_scaled = parameters.harmonics * 32767.0f;

  // Morph scales the chord intervals around Braids' voicing at 0.5.
  const float chord_width = parameters.morph * 2.0f;

  // The fourth macro's neutral position keeps Braids' decay.
  const float decay = kParticleStockDecay;

  // Correct the note once before entering Braids' pitch space.
  const float pitch128 = (parameters.note + kParticlePitchCorrection) * 128.0f;
  float pitch_offset128[kMaxBlockSize];
  fill(&pitch_offset128[0], &pitch_offset128[size], 0.0f);

  const float offset1 = kParticleFilter1Interval * 128.0f * chord_width;
  const float offset2 = kParticleFilter2Interval * 128.0f * chord_width;
  const float offset3 = kParticleFilter3Interval * 128.0f * chord_width;

  const int num_channels = stereo ? 2 : 1;

  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  for (int channel = 0; channel < num_channels; ++channel) {
    float amplitude = amplitude_[channel];
    float y11 = y1_state_[channel][0];
    float y12 = y1_state_[channel][1];
    float y21 = y2_state_[channel][0];
    float y22 = y2_state_[channel][1];
    float y31 = y3_state_[channel][0];
    float y32 = y3_state_[channel][1];
    float c1 = c1_[channel];
    float c2 = c2_[channel];
    float c3 = c3_[channel];
    float s1 = s1_[channel];
    float s2 = s2_[channel];
    float s3 = s3_[channel];

    for (size_t i = 0; i < size; ++i) {
      // One draw serves the trigger, jitter and excitation.
      const uint32_t noise = Random::GetWord();

      if ((noise & kParticleDensityMask) < density) {
        amplitude = 1.0f;

        const float noise_a = static_cast<float>(
            static_cast<int32_t>(noise & 0x0fffu) - 0x800);
        const float noise_b = static_cast<float>(
            static_cast<int32_t>((noise >> 15) & 0x1fffu) - 0x1000);

        // Truncate each pitch onto Braids' grid before reading its table.
        int32_t p1 = static_cast<int32_t>(
            pitch128 + pitch_offset128[i] + offset1 +
            3.0f * noise_a * harmonics_scaled / kParticleJitter1Divisor);
        CONSTRAIN(p1, 0, 16383);
        c1 = ResonatorCoefficient(p1);
        s1 = ResonatorScale(p1);

        int32_t p2 = static_cast<int32_t>(
            pitch128 + pitch_offset128[i] + offset2 +
            noise_a * harmonics_scaled / kParticleJitter2Divisor);
        CONSTRAIN(p2, 0, 16383);
        c2 = ResonatorCoefficient(p2);
        s2 = ResonatorScale(p2);

        int32_t p3 = static_cast<int32_t>(
            pitch128 + pitch_offset128[i] + offset3 +
            noise_b * harmonics_scaled / kParticleJitter3Divisor);
        CONSTRAIN(p3, 0, 16383);
        c3 = ResonatorCoefficient(p3);
        s3 = ResonatorScale(p3);
      }

      // Excitation uses the signed low sixteen bits of the same draw.
      const float sample = static_cast<float>(
          static_cast<int16_t>(noise & 0xffffu)) / 32768.0f * amplitude;
      amplitude *= decay;

      // Each resonator keeps Braids' uint16 scale and float recursion.
      float y10 = sample * s1 / 65536.0f + y11 * c1 -
          y12 * kParticleResonanceSquared;
      CONSTRAIN(y10, -1.0f, 1.0f);
      y12 = y11;
      y11 = y10;

      float y20 = sample * s2 / 65536.0f + y21 * c2 -
          y22 * kParticleResonanceSquared;
      CONSTRAIN(y20, -1.0f, 1.0f);
      y22 = y21;
      y21 = y20;

      float y30 = sample * s3 / 65536.0f + y31 * c3 -
          y32 * kParticleResonanceSquared;
      CONSTRAIN(y30, -1.0f, 1.0f);
      y32 = y31;
      y31 = y30;

      // Clip the sum after clipping each resonator.
      float sum = y10 + y20 + y30;
      CONSTRAIN(sum, -1.0f, 1.0f);

      if (channel == 0) {
        out[i] = sum;
        // Mono AUX is the raw exciter; overwritten below if stereo.
        aux[i] = sample;
      } else {
        aux[i] = sum;
      }
    }

    amplitude_[channel] = amplitude;
    y1_state_[channel][0] = y11;
    y1_state_[channel][1] = y12;
    y2_state_[channel][0] = y21;
    y2_state_[channel][1] = y22;
    y3_state_[channel][0] = y31;
    y3_state_[channel][1] = y32;
    c1_[channel] = c1;
    c2_[channel] = c2;
    c3_[channel] = c3;
    s1_[channel] = s1;
    s2_[channel] = s2;
    s3_[channel] = s3;
  }
}

}  // namespace plaits

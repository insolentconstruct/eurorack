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
// Morph: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/morph_engine.h"


#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

// Braids' ws_violent_overdrive, verbatim from braids/resources.cc.

const int16_t kViolentOverdrive[257] = {
  -32766, -32766, -32766, -32766, -32766, -32766, -32766, -32766,
  -32766, -32766, -32766, -32766, -32766, -32766, -32766, -32766,
  -32766, -32766, -32766, -32766, -32766, -32766, -32766, -32766,
  -32766, -32766, -32766, -32766, -32766, -32766, -32766, -32766,
  -32766, -32766, -32765, -32765, -32765, -32765, -32765, -32765,
  -32765, -32765, -32765, -32764, -32764, -32764, -32764, -32763,
  -32763, -32763, -32762, -32762, -32761, -32760, -32760, -32759,
  -32758, -32757, -32756, -32754, -32753, -32751, -32749, -32747,
  -32744, -32741, -32738, -32734, -32730, -32725, -32720, -32713,
  -32706, -32698, -32689, -32679, -32668, -32655, -32640, -32623,
  -32604, -32582, -32558, -32531, -32499, -32464, -32424, -32379,
  -32327, -32269, -32204, -32130, -32046, -31951, -31844, -31724,
  -31587, -31434, -31260, -31065, -30845, -30598, -30320, -30008,
  -29658, -29266, -28828, -28340, -27795, -27189, -26518, -25774,
  -24954, -24053, -23064, -21985, -20811, -19541, -18172, -16705,
  -15142, -13486, -11742,  -9919,  -8025,  -6073,  -4075,  -2045,
       0,   2045,   4075,   6073,   8025,   9919,  11742,  13486,
   15142,  16705,  18172,  19541,  20811,  21985,  23064,  24053,
   24954,  25774,  26518,  27189,  27795,  28340,  28828,  29266,
   29658,  30008,  30320,  30598,  30845,  31065,  31260,  31434,
   31587,  31724,  31844,  31951,  32046,  32130,  32204,  32269,
   32327,  32379,  32424,  32464,  32499,  32531,  32558,  32582,
   32604,  32623,  32640,  32655,  32668,  32679,  32689,  32698,
   32706,  32713,  32720,  32725,  32730,  32734,  32738,  32741,
   32744,  32747,  32749,  32751,  32753,  32754,  32756,  32757,
   32758,  32759,  32760,  32760,  32761,  32762,  32762,  32763,
   32763,  32763,  32764,  32764,  32764,  32764,  32765,  32765,
   32765,  32765,  32765,  32765,  32765,  32765,  32765,  32766,
   32766,  32766,  32766,  32766,  32766,  32766,  32766,  32766,
   32766,  32766,  32766,  32766,  32766,  32766,  32766,  32766,
   32766,  32766,  32766,  32766,  32766,  32766,  32766,  32766,
   32766,  32766,  32766,  32766,  32766,  32766,  32766,  32766,
   32766,
};

// Braids reads the shaper with Interpolate88(table, lp_state + 32768): the
inline float ReadShaper(float x) {
  // Saturate in the integer domain, as Interpolate88's int16 argument already
  const int32_t u = Clip16(static_cast<int32_t>(x * 32768.0f)) + 32768;
  const int integral = u >> 8;
  const float fractional = static_cast<float>(u & 0xff) * (1.0f / 256.0f);
  const float a = static_cast<float>(kViolentOverdrive[integral]);
  const float b = static_cast<float>(kViolentOverdrive[integral + 1]);
  return (a + (b - a) * fractional) * (1.0f / 32768.0f);
}

// Braids' triangle: a phase ramp folded about its midpoint, full scale, -1 at
inline float Triangle(float phase) {
  const float t = phase < 0.5f ? phase : 1.0f - phase;
  return 4.0f * t - 1.0f;
}

// lut_svf_cutoff's generator: f = cutoff / sample_rate, clamped at 1/8, then
inline float CutoffCoefficient(float cutoff_note) {
  float f = NoteToFrequency(cutoff_note) * 0.5f;
  if (f > kMorphCutoffLimit) {
    f = kMorphCutoffLimit;
  }
  // Sine(p) is sin(2*pi*p), so sin(pi*f) is Sine(f * 0.5).
  return 2.0f * Sine(f * 0.5f);
}

}  // namespace

// The 31-tap halfband, read out of a 32-entry ring of sub-samples. The caller
inline float Morph::Decimate(
    const float* history, int position) const {
  // Each logical entry is mirrored 32 floats later. Reading from the second
  const int p = position + 32;
  return kMorphHalfbandCentre * history[p - 16]
      + kMorphHalfband1 * (history[p - 15] + history[p - 17])
      + kMorphHalfband3 * (history[p - 13] + history[p - 19])
      + kMorphHalfband5 * (history[p - 11] + history[p - 21])
      + kMorphHalfband7 * (history[p - 9] + history[p - 23])
      + kMorphHalfband9 * (history[p - 7] + history[p - 25])
      + kMorphHalfband11 * (history[p - 5] + history[p - 27])
      + kMorphHalfband13 * (history[p - 3] + history[p - 29])
      + kMorphHalfband15 * (history[p - 1] + history[p - 31]);
}

void Morph::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Morph::Reset() {
  phase_ = 0.0f;
  frequency_ = 0.01f;
  saw_ = 0.0f;
  square_ = 0.0f;
  pw_ = 0.5f;
  fuzz_ = 0.0f;
  fuzz_aux_ = 0.0f;
  high_ = false;
  previous_pw_ = 0.5f;
  next_sample_ = 0.0f;
  lp_state_ = 0.0f;
  lp_state_aux_ = 0.0f;
  dc_input_ = 0.0f;
  dc_input_aux_ = 0.0f;
  for (int i = 0; i < 64; ++i) {
    history_[i] = 0.0f;
    history_aux_[i] = 0.0f;
  }
  history_position_ = 0;
}

void Morph::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    // Braids does NOT reset phase here: MacroOscillator::Strike
    phase_ = 0.0f;
    high_ = false;
    next_sample_ = 0.0f;
  }

  const bool stereo = false;

  float note = parameters.note;
  if (note > kMorphMaxNote) {
    note = kMorphMaxNote;
  }
  const float frequency = NoteToFrequency(note) * (1.0f / kMorphOversampling);

  // TIMBRE, macro_oscillator.cc:74-92. The branch is on the raw 15-bit
  const float p0 = parameters.timbre * 32767.0f;
  float target_saw;
  float target_square;
  float target_pw;
  if (p0 <= kMorphRegion1End) {
    // :77-79 -- triangle against saw.
    target_saw = p0 * kMorphBalanceSlope;
    target_square = 0.0f;
    target_pw = 0.5f;
  } else if (p0 <= kMorphRegion2End) {
    // :83-85 -- saw against a 50% square, running backwards.
    target_saw = 1.0f - (p0 - kMorphRegion2Start) * kMorphBalanceSlope;
    target_square = 1.0f - target_saw;
    target_pw = 0.5f;
  } else {
    // :87-91 -- the square alone, narrowing. The sine the model asks for on
    float square_parameter = (p0 - kMorphRegion3Start) * kMorphSquareSlope;
    if (square_parameter > kMorphSquareMax) {
      square_parameter = kMorphSquareMax;
    }
    target_saw = 0.0f;
    target_square = 1.0f;
    target_pw = (32768.0f - square_parameter) * (1.0f / 65536.0f);
  }
  // A floor only so the pulse can never reach zero width. Chaining the two
  float pw_floor = 0.5f * frequency;
  if (pw_floor > 0.25f) {
    pw_floor = 0.25f;
  }
  if (target_pw < pw_floor) {
    target_pw = pw_floor;
  }

  // COLOR, :99 and :107. Both jobs come off the same knob; MORPH then offsets
  const float tone_offset = (parameters.morph - 0.5f) * 2.0f *
      kMorphMaxToneOffset;

  // :107-113 -- the fuzz mix, rolled off above MIDI 80.
  float fuzz_position = parameters.harmonics;
  float fuzz_position_aux = fuzz_position;
  // In stereo the two channels sit either side of the Fuzz knob. The knob is
  if (stereo) {
    const float centre = kMorphStereoFuzz +
        fuzz_position * (1.0f - 2.0f * kMorphStereoFuzz);
    fuzz_position = centre - kMorphStereoFuzz;
    fuzz_position_aux = centre + kMorphStereoFuzz;
  }

  float target_fuzz = fuzz_position;
  float target_fuzz_aux = fuzz_position_aux;
  if (note > kMorphFuzzGuardNote) {
    const float roll_off = (note - kMorphFuzzGuardNote) * kMorphFuzzRollOff;
    target_fuzz -= roll_off;
    target_fuzz_aux -= roll_off;
  }
  CONSTRAIN(target_fuzz, 0.0f, 1.0f);
  CONSTRAIN(target_fuzz_aux, 0.0f, 1.0f);

  // The cutoff travels with the fuzz exactly as Braids welds them, so in
  float cutoff_note = note + kMorphCutoffCeiling -
      fuzz_position * kMorphCutoffSpan + tone_offset;
  float cutoff_note_aux = note + kMorphCutoffCeiling -
      fuzz_position_aux * kMorphCutoffSpan + tone_offset;
  CONSTRAIN(cutoff_note, 0.0f, kMorphCutoffMax);
  CONSTRAIN(cutoff_note_aux, 0.0f, kMorphCutoffMax);
  const float lp_a = CutoffCoefficient(cutoff_note);
  const float lp_a_aux = stereo ? CutoffCoefficient(cutoff_note_aux) : 0.0f;

  const float drive = 1.0f;

  // One shared 1 / size: initialised with a size, each interpolator
  const float step = 1.0f / static_cast<float>(size);
  ParameterInterpolator fm(&frequency_, frequency, step);
  ParameterInterpolator saw_modulation(&saw_, target_saw, step);
  ParameterInterpolator square_modulation(&square_, target_square, step);
  ParameterInterpolator pwm(&pw_, target_pw, step);
  ParameterInterpolator fuzz_modulation(&fuzz_, target_fuzz, step);
  ParameterInterpolator fuzz_modulation_aux(&fuzz_aux_, target_fuzz_aux, step);

  float next_sample = next_sample_;
  // The oscillator, filter and DC states run in locals, written back after
  float phase = phase_;
  bool high = high_;
  float previous_pw = previous_pw_;
  float lp_state = lp_state_;
  float lp_state_aux = lp_state_aux_;
  float dc_input = dc_input_;
  float dc_input_aux = dc_input_aux_;
  int history_position = history_position_;

  for (size_t i = 0; i < size; ++i) {
    float f = fm.Next();
    const float w_saw = saw_modulation.Next();
    const float w_square = square_modulation.Next();
    const float w_triangle = 1.0f - w_saw - w_square;
    const float pw = pwm.Next();
    const float fuzz = fuzz_modulation.Next();
    const float fuzz_aux = fuzz_modulation_aux.Next();

    for (int j = 0; j < kMorphOversampling; ++j) {
      float this_sample = next_sample;
      next_sample = 0.0f;

    {
      phase += f;

      if (!high && phase >= pw) {
        // The square's rising edge. Only the square has one here.
        float t = (phase - pw) / (previous_pw - pw + f);
        CONSTRAIN(t, 0.0f, 1.0f);
        const float step = 2.0f * w_square;
        this_sample += step * ThisBlepSample(t);
        next_sample += step * NextBlepSample(t);
        high = true;
      }
      // NOT an `else`: at the top of TIMBRE the pulse is 1.2% of a cycle, so
      if (phase >= 1.0f) {
        phase -= 1.0f;
        float t = phase / f;
        CONSTRAIN(t, 0.0f, 1.0f);
        // The saw drops from +1 to -1 and, in the same instant, so does the
        const float step = -2.0f * (w_saw + (high ? w_square : 0.0f));
        this_sample += step * ThisBlepSample(t);
        next_sample += step * NextBlepSample(t);
        high = false;
      }

    }
      // Braids' triangle is itself 2x oversampled and box-averaged
      float half_phase = phase - 0.5f * f;
      if (half_phase < 0.0f) {
        half_phase += 1.0f;
      }
      const float triangle =
          0.5f * (Triangle(half_phase) + Triangle(phase));

      const float naive = w_saw * (2.0f * phase - 1.0f) +
          w_square * (phase < pw ? -1.0f : 1.0f) +
          w_triangle * triangle;
      next_sample += naive;
      previous_pw = pw;

      // The morph oscillator, band-limited. Braids filters and fuzzes THIS.
      const float sample = this_sample;

      // :117-118 -- the one-pole, with Braids' own clip on the state. The
      lp_state += (sample - lp_state) * lp_a;
      CONSTRAIN(lp_state, -1.0f, 1.0f);
      // :121-122 -- the shaper reads the filter state, and the result is
      const float fuzzed = ReadShaper(lp_state * drive);
      const float processed = sample + (fuzzed - sample) * fuzz;
      history_[history_position] = processed;
      history_[history_position + 32] = history_[history_position];

      if (stereo) {
        lp_state_aux += (sample - lp_state_aux) * lp_a_aux;
        CONSTRAIN(lp_state_aux, -1.0f, 1.0f);
        const float fuzzed_aux = ReadShaper(lp_state_aux * drive);
        const float processed_aux =
            sample + (fuzzed_aux - sample) * fuzz_aux;
        history_aux_[history_position] = processed_aux;
      } else {
        // Mono AUX is the morph oscillator dry: the analog waveform the model
        history_aux_[history_position] = sample;
      }
      history_aux_[history_position + 32] =
          history_aux_[history_position];

      history_position = (history_position + 1) & 31;
    }

    const float raw = Decimate(history_, history_position);
    const float raw_aux = Decimate(history_aux_, history_position);

    // Braids has no DC blocker, and at the top of TIMBRE its pulse carries up
    ONE_POLE(dc_input, raw, 0.001f);
    ONE_POLE(dc_input_aux, raw_aux, 0.001f);

    out[i] = raw - dc_input;
    aux[i] = raw_aux - dc_input_aux;
  }

  phase_ = phase;
  high_ = high;
  previous_pw_ = previous_pw;
  lp_state_ = lp_state;
  lp_state_aux_ = lp_state_aux;
  dc_input_ = dc_input;
  dc_input_aux_ = dc_input_aux;
  history_position_ = history_position;
  next_sample_ = next_sample;
}

}  // namespace plaits

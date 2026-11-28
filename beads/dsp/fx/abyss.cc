// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// A modulated feedback delay network using the recording banks.

#include "beads/dsp/fx/abyss.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

namespace beads {

using namespace std;

namespace {

const float kMaxDiffusion = 0.67f;
const float kSemitoneToLogRatio = 0.057762265f;
const float kPitchDetent = 1.0f / 18.0f;
const float kDetuneEnd = 0.33f;
const float kFifthStart = 0.55f;
const float kFifthEnd = 0.68f;
const float kOctaveStart = 0.94f;
const float kPitchMix = 0.2f;
const float kFifth = 7.0f;
const float kOctave = 12.0f;

float Ramp(float value, float start, float end) {
  float x = (value - start) / (end - start);
  CONSTRAIN(x, 0.0f, 1.0f);
  return stmlib::SmoothStep(x);
}

const float line_time[] = {
  0.151f, 0.181f, 0.203f, 0.239f, 0.277f, 0.313f, 0.359f, 0.409f
};
const float diffuser_time[] = { 0.0061f, 0.0193f, 0.0097f, 0.0239f };
const float drift_rate[] = {
  0.131f, 0.173f, 0.227f, 0.199f, 0.283f, 0.109f, 0.251f, 0.157f
};

}  // namespace

void Abyss::Init(
    uint8_t* bank_0, uint8_t* bank_1, float rate) {
  float* banks[] = { reinterpret_cast<float*>(bank_0),
      reinterpret_cast<float*>(bank_1) };
  for (int i = 0; i < kLines; ++i) {
    lines_[i] = i < kLinesInBank0
        ? banks[0] + i * (kLineSize + kDiffuserSize)
        : banks[1] + (i - kLinesInBank0) * kLineSize;
    fill(lines_[i], lines_[i] + kLineSize, 0.0f);
    damping_[i] = 0.0f;
    phase_[i] = float(i * 3 % kLines) / float(kLines);
    frozen_delay_[i] = 0.0f;
  }
  for (int i = 0; i < 4; ++i) {
    fill(lines_[i] + kLineSize, lines_[i] + kLineSize + kDiffuserSize, 0.0f);
  }
  sample_rate_ = rate;
  room_slew_ = 1.0f / (0.5f * rate);
  control_slew_ = 1.0f / (0.008f * rate);
  const float corner = 6283.1853f;
  damping_coefficient_ = corner / (rate + 0.5f * corner);
  room_ = -1.0f;
  wet_ = 0.0f;
  frozen_ = 0.0f;
  onset_coefficient_ = 1.0f / (0.22f * rate);
  onset_floor_ = 0.0f;
  elapsed_ = 8.0f;
  fill(pitch_phase_, pitch_phase_ + 6, 0.0f);
  detune_step_[0] = detune_step_[1] = 0.0f;
  fill(pitch_mix_, pitch_mix_ + 5, 0.0f);
  gravity_gain_ = 1.0f;
  was_frozen_ = false;
  write_ = 0;
  diffusion_write_ = 0;
}

float Abyss::Read(int line, float delay) const {
  const int integral = int(delay);
  const float fraction = delay - float(integral);
  int index = write_ - integral;
  if (index < 0) {
    index += kLineSize;
  }
  const int previous = index ? index - 1 : kLineSize - 1;
  const float* samples = lines_[line];
  const float a = samples[index];
  return a + fraction * (samples[previous] - a);
}

float Abyss::Diffuse(int stage, float input, int delay, float amount) {
  float* buffer = lines_[stage] + kLineSize;
  // The stage's storage follows its line, beyond the maximum read distance.
  const int read = (diffusion_write_ - delay) & (kDiffuserSize - 1);
  const float delayed = buffer[read];
  const float sum = input + amount * delayed;
  buffer[diffusion_write_] = sum;
  const float all_pass = delayed - amount * sum;
  const float blend = amount / kMaxDiffusion;
  return input + blend * (all_pass - input);
}

void Abyss::Mix(float* taps) const {
  for (int stride = 1; stride < kLines; stride *= 2) {
    for (int base = 0; base < kLines; base += 2 * stride) {
      for (int i = base; i < base + stride; ++i) {
        const float a = taps[i];
        const float b = taps[i + stride];
        taps[i] = a + b;
        taps[i + stride] = a - b;
      }
    }
  }
}

/* static */
bool Abyss::PitchOnNotch(float position) {
  return fabsf(position) <= kPitchDetent;
}

/* static */
Abyss::Pitch Abyss::MapPitch(float position) {
  const float distance = fabsf(position);
  const float detune = Ramp(distance, kPitchDetent, kDetuneEnd);
  const float fifth = Ramp(distance, kDetuneEnd, kFifthStart);
  const float octave = Ramp(distance, kFifthEnd, kOctaveStart);
  const float sign = position < 0.0f ? -1.0f : 1.0f;
  Pitch pitch;
  pitch.detune = 0.09f * detune;
  pitch.detune_mix = kPitchMix * detune * (1.0f - fifth);
  pitch.fifth = kFifth * sign;
  pitch.fifth_mix = kPitchMix * fifth * (1.0f - octave);
  pitch.octave = kOctave * sign;
  pitch.octave_mix = kPitchMix * octave;
  return pitch;
}

float Abyss::Shift(float* phase, float step, float base, float width) const {
  float p = *phase + step;
  if (p < 0.0f) {
    p += 1.0f;
  } else if (p >= 1.0f) {
    p -= 1.0f;
  }
  *phase = p;
  const float other = p < 0.5f ? p + 0.5f : p - 0.5f;
  const float blend = 1.0f - 2.0f * fabsf(p - 0.5f);
  return blend * Read(0, base + width * p)
      + (1.0f - blend) * Read(0, base + width * other);
}

void Abyss::Process(const Controls& c, const bool* freeze,
    const FloatFrame* in, FloatFrame* out, size_t size) {
  const float room_target = 0.24f + 0.76f * c.room;
  if (room_ < 0.0f) {
    room_ = room_target;
    wet_ = c.mix;
  }
  const float rt60 = 0.8f + 23.2f * c.decay * c.decay;
  const float seconds = 1.0f / sample_rate_;
  const float intensity = kMaxDiffusion * c.diffusion;
  const float drift = c.drift * c.drift;
  const float depth = 0.003f * drift * sample_rate_;
  const float gravity_position = 2.0f * c.gravity - 1.0f;
  const float gravity = copysignf(
      max(0.0f, fabsf(gravity_position) - 0.015f) / 0.985f, gravity_position);
  const float swell = 0.55f + 2.45f * c.room * min(1.0f, rt60 / 3.0f);
  const float tilt = 2.0f * c.tone - 1.0f;
  const float low_gain = 0.96f - 0.35f * max(tilt, 0.0f);
  const float high_gain = 0.91f - 0.54f * max(-tilt, 0.0f)
      + 0.08f * max(tilt, 0.0f);
  const float pitch_width = 0.035f * sample_rate_;
  const Pitch pitch = MapPitch(c.pitch);
  const float target[] = {
    pitch.detune_mix,
    pitch.fifth > 0.0f ? pitch.fifth_mix : 0.0f,
    pitch.fifth < 0.0f ? pitch.fifth_mix : 0.0f,
    pitch.octave > 0.0f ? pitch.octave_mix : 0.0f,
    pitch.octave < 0.0f ? pitch.octave_mix : 0.0f
  };
  const float intervals[] = { kFifth, -kFifth, kOctave, -kOctave };
  float pitch_step[6] = { 0.0f };
  bool pitch_active = false;
  for (int i = 0; i < 5; ++i) {
    pitch_active |= target[i] > 0.0f || pitch_mix_[i] > 0.0f;
  }
  if (pitch_active) {
    // Approximate exp(x) - 1 for detune without quantizing either voice.
    const float x = pitch.detune * kSemitoneToLogRatio;
    const float quadratic = 0.5f * x * x;
    const float inverse_width = 1.0f / pitch_width;
    pitch_step[0] = (-x - quadratic) * inverse_width;
    pitch_step[1] = (x - quadratic) * inverse_width;
    for (int i = 0; i < 4; ++i) {
      pitch_step[i + 2] =
          (1.0f - stmlib::SemitonesToRatio(intervals[i])) * inverse_width;
    }
  }
  float gain[kLines];
  float length[kLines];
  float phase_step[kLines];
  int diffusion_delay[4];
  for (int i = 0; i < kLines; ++i) {
    // 60 dB in rt60 seconds, scaled to each line's round trip.
    gain[i] = stmlib::SemitonesToRatio(-119.5894f * line_time[i] * room_ / rt60);
    length[i] = line_time[i] * sample_rate_;
    phase_step[i] = drift_rate[i] * (0.3f + 2.7f * c.drift) * seconds;
  }
  for (int i = 0; i < 4; ++i) {
    diffusion_delay[i] = int(diffuser_time[i] * sample_rate_);
  }

  for (size_t i = 0; i < size; ++i) {
    room_ += room_slew_ * (room_target - room_) * (1.0f - frozen_);
    wet_ += control_slew_ * (c.mix - wet_);
    frozen_ += freeze[i] ? 100.0f * seconds : -100.0f * seconds;
    CONSTRAIN(frozen_, 0.0f, 1.0f);

    const float peak = max(fabsf(in[i].l), fabsf(in[i].r));
    if (peak > max(0.003f, 3.0f * onset_floor_) &&
        elapsed_ > 0.065f && frozen_ == 0.0f) {
      elapsed_ = 0.0f;
    }
    onset_floor_ += onset_coefficient_ * (peak - onset_floor_);
    elapsed_ = min(elapsed_ + seconds, 8.0f);

    const float feedback_input = 1.0f - frozen_;
    const float input_gain = 0.23f * feedback_input;
    const float left = Diffuse(1, Diffuse(0, feedback_input * in[i].l,
        diffusion_delay[0], intensity), diffusion_delay[1], intensity);
    const float right = Diffuse(3, Diffuse(2, feedback_input * in[i].r,
        diffusion_delay[2], intensity), diffusion_delay[3], intensity);

    float taps[kLines];
    for (int j = 0; j < kLines; ++j) {
      phase_[j] += phase_step[j];
      if (phase_[j] >= 1.0f) {
        phase_[j] -= 1.0f;
      }
      const float triangle = 1.0f - 4.0f * fabsf(phase_[j] - 0.5f);
      const float delay = length[j] * room_;
      if (freeze[i] && !was_frozen_) {
        frozen_delay_[j] = float(int(delay + 0.5f));
      }
      float tap;
      if (frozen_ == 1.0f) {
        tap = Read(j, frozen_delay_[j]);
      } else {
        tap = Read(j, delay + depth * triangle);
        if (frozen_ > 0.0f) {
          tap += frozen_ * (Read(j, frozen_delay_[j]) - tap);
        }
      }
      taps[j] = tap;
    }

    const float wet_l = (taps[0] - taps[2] + taps[5] + taps[7]) * 0.5f;
    const float wet_r = (taps[1] - taps[3] + taps[4] - taps[6]) * 0.5f;

    if (pitch_active) {
      for (int j = 0; j < 2; ++j) {
        detune_step_[j] += control_slew_ * (pitch_step[j] - detune_step_[j]);
      }
      for (int j = 0; j < 5; ++j) {
        pitch_mix_[j] += control_slew_ * (target[j] - pitch_mix_[j]);
        if (pitch_mix_[j] < 1e-6f && target[j] == 0.0f) {
          pitch_mix_[j] = 0.0f;
        }
      }
      const float base = length[0] * room_;
      float pitch_feedback = 0.0f;
      if (frozen_ < 1.0f) {
        if (pitch_mix_[0] > 0.0f) {
          const float up = Shift(&pitch_phase_[0], detune_step_[0],
              base, pitch_width);
          const float down = Shift(&pitch_phase_[1], detune_step_[1],
              base, pitch_width);
          pitch_feedback += pitch_mix_[0] * (0.5f * (up + down) - taps[0]);
        }
        for (int j = 1; j < 5; ++j) {
          if (pitch_mix_[j] > 0.0f) {
            const float shifted = Shift(&pitch_phase_[j + 1], pitch_step[j + 1],
                base, pitch_width);
            pitch_feedback += pitch_mix_[j] * (shifted - taps[0]);
          }
        }
      }
      if (pitch_feedback != 0.0f) {
        taps[0] += feedback_input * pitch_feedback;
      }
    }

    Mix(taps);
    for (int j = 0; j < kLines; ++j) {
      const float tap = 0.35355339f * taps[j];
      damping_[j] += damping_coefficient_ * (tap - damping_[j]);
      const float damped = high_gain * tap + (low_gain - high_gain) * damping_[j];
      const float excitation = j & 1 ? right : left;
      const float sign = j & 2 ? -1.0f : 1.0f;
      float sample = input_gain * sign * excitation
          + (1.0f - frozen_) * gain[j] * damped + frozen_ * tap;
      CONSTRAIN(sample, -4.0f, 4.0f);
      lines_[j][write_] = sample;
    }

    // Gravity shapes the audible tail while the network continues to diffuse.
    const float age = elapsed_ / swell;
    const float bloom = min(1.0f, age);
    const float cutoff = min(1.0f, max(0.0f, 16.0f * (1.0f - age)));
    const float inverse = bloom * cutoff;
    const float envelope = gravity < 0.0f ? bloom * bloom : inverse * inverse;
    gravity_gain_ += control_slew_ * (envelope - gravity_gain_);
    const float wet = wet_ * (1.0f - feedback_input *
        fabsf(gravity) * (1.0f - gravity_gain_));
    out[i].l = (1.0f - wet_) * in[i].l + wet * wet_l;
    out[i].r = (1.0f - wet_) * in[i].r + wet * wet_r;
    was_frozen_ = freeze[i];
    if (++write_ == kLineSize) {
      write_ = 0;
    }
    diffusion_write_ = (diffusion_write_ + 1) & (kDiffuserSize - 1);
  }
}

}  // namespace beads

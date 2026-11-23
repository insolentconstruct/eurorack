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
// DualSync: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/dual_sync_engine.h"


#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

#include "plaits/dsp/oscillator/oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

// 47-tap halfband, Kaiser window beta = 12, cutoff at half the 96 kHz internal
const float kHalfbandCenter = 0.4999997483f;

const float kHalfbandTaps[kDualSyncHalfbandPairs] = {
  -0.0000007304f, +0.0000198297f, -0.0001196836f, +0.0004564988f,
  -0.0013417914f, +0.0033070347f, -0.0071726082f, +0.0141749919f,
  -0.0263829328f, +0.0483750742f, -0.0961859865f, +0.3148704295f
};

// Braids' square is `phase < pw ? 0 : 32767` and its saw is `phase >> 17`,
inline float NaiveShape(float phase, float square_amount, float saw_amount) {
  const float square = phase < kDualSyncPulseWidth ? 0.0f : 1.0f;
  return square_amount * square + saw_amount * phase;
}

}  // namespace

void DualSync::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void DualSync::Reset() {
  master_phase_ = 0.0f;
  slave_phase_ = 0.0f;
  master_high_ = false;
  slave_high_ = false;
  master_next_sample_ = 0.0f;
  slave_next_sample_ = 0.0f;

  master_frequency_ = 0.005f;
  slave_frequency_ = 0.005f;
  balance_ = 0.0f;
  shape_ = 0.0f;
  reset_phase_ = 0.0f;

  // SPEC R15: another engine's buffers alias these addresses, so every tap has
  for (int i = 0; i < kDualSyncDecimatorSize; ++i) {
    master_history_[i] = 0.0f;
    slave_history_[i] = 0.0f;
  }
  decimator_write_ = 0;
}

void DualSync::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    master_phase_ = 0.0f;
    slave_phase_ = 0.0f;
    master_high_ = false;
    slave_high_ = false;
    master_next_sample_ = 0.0f;
    slave_next_sample_ = 0.0f;
  }

  const bool stereo = false;

  // TIMBRE is the sync interval; COLOR is the master/slave balance.
  const float interval = parameters.timbre * kDualSyncMaxInterval;

  // The increments are internal, i.e. at 2x the output rate, so they are half
  const float internal_ceiling = kMaxFrequency / float(kDualSyncOversampling);
  float master_frequency = NoteToFrequency(parameters.note) /
      float(kDualSyncOversampling);
  float slave_frequency = NoteToFrequency(parameters.note + interval) /
      float(kDualSyncOversampling);
  CONSTRAIN(master_frequency, 0.0f, internal_ceiling);
  CONSTRAIN(slave_frequency, 0.0f, internal_ceiling);

  const float target_balance = parameters.harmonics;
  const float target_shape = parameters.morph;

  // Braids restarts the slave at phase zero; MACRO moves that restart point
  const float target_reset_phase = 0.0f;

  ParameterInterpolator master_fm(&master_frequency_, master_frequency, size);
  ParameterInterpolator slave_fm(&slave_frequency_, slave_frequency, size);
  ParameterInterpolator balance_modulation(&balance_, target_balance, size);
  ParameterInterpolator shape_modulation(&shape_, target_shape, size);
  ParameterInterpolator reset_modulation(
      &reset_phase_, target_reset_phase, size);

  float master_next = master_next_sample_;
  float slave_next = slave_next_sample_;

  for (size_t i = 0; i < size; ++i) {
    float mf = master_fm.Next();
    float sf = slave_fm.Next();
    const float balance = balance_modulation.Next();
    const float shape = shape_modulation.Next();

    const float saw_amount = shape;
    const float square_amount = 1.0f - shape;

    float reset_phase = reset_modulation.Next();
    if (reset_phase < 0.0f) {
      reset_phase += 1.0f;
    }
    const float reset_value = NaiveShape(
        reset_phase, square_amount, saw_amount);

    for (int j = 0; j < kDualSyncOversampling; ++j) {
      float master_this = master_next;
      float slave_this = slave_next;
      master_next = 0.0f;
      slave_next = 0.0f;

    {
      // --- master -------------------------------------------------------
      bool sync = false;
      float reset_time = 0.0f;

      master_phase_ += mf;
      if (!master_high_ && master_phase_ >= kDualSyncPulseWidth) {
        float t = (master_phase_ - kDualSyncPulseWidth) / mf;
        CONSTRAIN(t, 0.0f, 1.0f);
        master_this += square_amount * ThisBlepSample(t);
        master_next += square_amount * NextBlepSample(t);
        master_high_ = true;
      }
      if (master_phase_ >= 1.0f) {
        master_phase_ -= 1.0f;
        reset_time = master_phase_ / mf;
        CONSTRAIN(reset_time, 0.0f, 1.0f);
        // Square and saw both fall by a full unit here, so the step is 1
        master_this -= ThisBlepSample(reset_time);
        master_next -= NextBlepSample(reset_time);
        master_high_ = false;
        sync = true;
      }
      master_next += NaiveShape(master_phase_, square_amount, saw_amount);

      // --- slave --------------------------------------------------------
      bool transition_during_reset = false;
      if (sync) {
        // Where the slave WOULD have been at the reset instant. Braids builds
        float phase_at_reset = slave_phase_ + (1.0f - reset_time) * sf;
        if (phase_at_reset >= 1.0f) {
          phase_at_reset -= 1.0f;
          transition_during_reset = true;
        }
        if (!slave_high_ && phase_at_reset >= kDualSyncPulseWidth) {
          transition_during_reset = true;
        }
        const float step = reset_value - NaiveShape(
            phase_at_reset, square_amount, saw_amount);
        slave_this += step * ThisBlepSample(reset_time);
        slave_next += step * NextBlepSample(reset_time);
      }

      slave_phase_ += sf;
      while (transition_during_reset || !sync) {
        if (!slave_high_) {
          if (slave_phase_ < kDualSyncPulseWidth) {
            break;
          }
          float t = (slave_phase_ - kDualSyncPulseWidth) / sf;
          CONSTRAIN(t, 0.0f, 1.0f);
          slave_this += square_amount * ThisBlepSample(t);
          slave_next += square_amount * NextBlepSample(t);
          slave_high_ = true;
        }
        if (slave_high_) {
          if (slave_phase_ < 1.0f) {
            break;
          }
          slave_phase_ -= 1.0f;
          float t = slave_phase_ / sf;
          CONSTRAIN(t, 0.0f, 1.0f);
          slave_this -= ThisBlepSample(t);
          slave_next -= NextBlepSample(t);
          slave_high_ = false;
        }
      }

      if (sync) {
        slave_phase_ = reset_phase + reset_time * sf;
        if (slave_phase_ >= 1.0f) {
          slave_phase_ -= 1.0f;
        }
        // Braids sets high_ = false because it always restarts at zero, which
        slave_high_ = slave_phase_ >= kDualSyncPulseWidth;
      }
      slave_next += NaiveShape(slave_phase_, square_amount, saw_amount);

    }
      // --- 2x -> 1x -----------------------------------------------------
      master_history_[decimator_write_] = 2.0f * master_this - 1.0f;
      slave_history_[decimator_write_] = 2.0f * slave_this - 1.0f;
      decimator_write_ = (decimator_write_ + 1) & (kDualSyncDecimatorSize - 1);
    }

    // Straight FIR over the 47 samples ending at the one just written, oldest
    const int mask = kDualSyncDecimatorSize - 1;
    const int base = (decimator_write_ + kDualSyncDecimatorSize -
        kDualSyncHalfbandLength) & mask;
    const int centre = (base + (kDualSyncHalfbandLength - 1) / 2) & mask;
    float master_sum = kHalfbandCenter * master_history_[centre];
    float slave_sum = kHalfbandCenter * slave_history_[centre];
    for (int tap = 0; tap < kDualSyncHalfbandPairs; ++tap) {
      const int low = (base + 2 * tap) & mask;
      const int high = (base + kDualSyncHalfbandLength - 1 - 2 * tap) & mask;
      master_sum += kHalfbandTaps[tap] *
          (master_history_[low] + master_history_[high]);
      slave_sum += kHalfbandTaps[tap] *
          (slave_history_[low] + slave_history_[high]);
    }

    // Braids mixes at 96 kHz and the reference decimates afterwards; both
    float balance_main = balance;
    float balance_aux = 1.0f - balance;
    if (stereo) {
      balance_main = balance - kDualSyncStereoBalance;
      balance_aux = balance + kDualSyncStereoBalance;
      CONSTRAIN(balance_main, 0.0f, 1.0f);
      CONSTRAIN(balance_aux, 0.0f, 1.0f);
    }

    const float difference = slave_sum - master_sum;
    out[i] = kDualSyncOutputGain * (master_sum + difference * balance_main);
    aux[i] = kDualSyncOutputGain * (master_sum + difference * balance_aux);
  }

  master_next_sample_ = master_next;
  slave_next_sample_ = slave_next;
}

}  // namespace plaits
